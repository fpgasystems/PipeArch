`include "pipearch_common.vh"

module glm_modify
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [7],

    fifobram_interface.read MEM_labels_read,
    fifobram_interface.read FIFO_dot_read,
    fifobram_interface.write MEM_labels_write,
    fifobram_interface.write REGION_gradient_write
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_SGD_MAIN,
        STATE_SCD_MAIN
    } t_modifystate;
    t_modifystate modify_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_iterations;
    logic [15:0] offset_by_index;
    logic [15:0] offset_by_index_write;
    logic [3:0] position_by_index;
    logic [3:0] write_position_by_index;
    logic [15:0] MEM_labels_read_load_offset;
    logic [1:0] model_type;
    logic algorithm_type;
    logic [31:0] step_size;
    logic [31:0] lambda;
    logic [CLDATA_WIDTH-1:0] line_from_MEM_labels;
    logic [31:0] REGION_gradient_write_accessproperties;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_performed_iterations;
    logic [15:0] write_num_performed_iterations;
    logic write_trigger;

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    typedef struct packed {
        logic       trigger;
        logic[31:0] leftoperand;
        logic[31:0] rightoperand;
        logic[31:0] result;
        logic       done;
    } fp_compute_regs;

    fp_compute_regs sub_regs;
    float_subtract
    subtract (
        .clk(clk),
        .reset(reset),
        .in1(sub_regs.leftoperand),
        .in2(sub_regs.rightoperand),
        .in_valid(sub_regs.trigger),
        .q(sub_regs.result),
        .q_valid(sub_regs.done));

    fp_compute_regs mult_regs;
    logic mult_regs_trigger_1d;
    float_mult
    multiply (
        .clk(clk),
        .reset(reset),
        .in1(mult_regs.leftoperand),
        .in2(mult_regs.rightoperand),
        .in_valid(mult_regs.trigger),
        .q(mult_regs.result),
        .q_valid(mult_regs.done));

    fp_compute_regs l1reg_regs;
    float_l1reg
    l1reg (
        .clk(clk),
        .reset(reset),
        .in_model(l1reg_regs.leftoperand),
        .in_step(l1reg_regs.rightoperand),
        .in_lambda(lambda),
        .in_valid(l1reg_regs.trigger),
        .out_step(l1reg_regs.result),
        .out_valid(l1reg_regs.done));

    // *************************************************************************
    //
    //   Write Channels
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(1)) dummy_accessprops_read();

    internal_interface #(.WIDTH(32)) from_modify_to_output();
    write_region
    write_REGION_dot_write (
        .clk, .reset,
        .op_start(write_trigger),
        .configreg(REGION_gradient_write_accessproperties),
        .iterations(num_iterations),
        .into_write(from_modify_to_output.commonwrite_source),
        .props_access(dummy_accessprops_read.read),
        .region_access(REGION_gradient_write)
    );

    always_ff @(posedge clk)
    begin
        write_trigger <= 1'b0;
        sub_regs.trigger <= 1'b0;
        l1reg_regs.trigger <= 1'b0;
        mult_regs.trigger <= 1'b0;
        mult_regs_trigger_1d <= mult_regs.trigger;
        FIFO_dot_read.re <= 1'b0;
        FIFO_dot_read.rfifobram <= 2'b10;
        MEM_labels_read.re <= 1'b0;
        MEM_labels_read.rfifobram <= 2'b01;
        MEM_labels_write.we <= 1'b0;
        MEM_labels_write.wfifobram <= 2'b01;
        from_modify_to_output.we <= 1'b0;
        op_done <= 1'b0;

        case (modify_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    offset_by_index <= regs[3][3] ? regs[0][15:0] : 0;
                    offset_by_index_write <= regs[3][3] ? regs[0][15:0] : 0;
                    position_by_index <= regs[3][3] ? regs[0][3:0] : 0;
                    write_position_by_index <= regs[3][3] ? regs[0][3:0] : 0;
                    num_iterations <= regs[1][31:16];
                    MEM_labels_read_load_offset <= regs[2][15:0];
                    model_type <= regs[3][1:0];
                    algorithm_type <= regs[3][2];
                    step_size <= regs[4];
                    lambda <= regs[5];
                    REGION_gradient_write_accessproperties <= regs[6];
                    // *************************************************************************
                    num_performed_iterations <= 0;
                    write_num_performed_iterations <= 0;
                    modify_state <= (regs[3][2] == 1'b0) ? STATE_SGD_MAIN : STATE_SCD_MAIN;
                    write_trigger <= 1'b1;
                end
            end

            STATE_SGD_MAIN:
            begin
                if (!FIFO_dot_read.empty && num_performed_iterations < num_iterations && !from_modify_to_output.almostfull)
                begin
                    FIFO_dot_read.re <= 1'b1;
                    MEM_labels_read.re <= 1'b1;
                    MEM_labels_read.raddr <= MEM_labels_read_load_offset + (offset_by_index[15:4]);
                    num_performed_iterations <= num_performed_iterations + 1;
                    offset_by_index <= offset_by_index + 1;
                end

                if (FIFO_dot_read.rvalid && MEM_labels_read.rvalid)
                begin
                    sub_regs.trigger <= 1'b1;
                    sub_regs.leftoperand <= FIFO_dot_read.rdata[31:0];
                    sub_regs.rightoperand <= MEM_labels_read.rdata[position_by_index*32+31 -: 32];
                    position_by_index <= position_by_index + 1;
                end

                if (sub_regs.done)
                begin
                    mult_regs.trigger <= 1'b1;
                    mult_regs.leftoperand <= step_size;
                    mult_regs.rightoperand <= sub_regs.result;
                end

                if (mult_regs.done)
                begin
                    from_modify_to_output.we <= 1'b1;
                    from_modify_to_output.wdata <= mult_regs.result;
                    write_num_performed_iterations <= write_num_performed_iterations + 1;
                    if (write_num_performed_iterations == num_iterations-1)
                    begin
                        op_done <= 1'b1;
                        modify_state <= STATE_IDLE;
                    end
                end
            end

            STATE_SCD_MAIN:
            begin
                if (!FIFO_dot_read.empty && num_performed_iterations < num_iterations && !from_modify_to_output.almostfull)
                begin
                    FIFO_dot_read.re <= 1'b1;
                    num_performed_iterations <= num_performed_iterations + 1;
                end

                if (FIFO_dot_read.rvalid)
                begin
                    mult_regs.trigger <= 1'b1;
                    mult_regs.leftoperand <= step_size;
                    mult_regs.rightoperand <= FIFO_dot_read.rdata[31:0];
                end

                if (mult_regs_trigger_1d)
                begin
                    MEM_labels_read.re <= 1'b1;
                    MEM_labels_read.raddr <= MEM_labels_read_load_offset + (offset_by_index[15:4]);
                    offset_by_index <= offset_by_index + 1;
                end

                if (mult_regs.done)
                begin
                    l1reg_regs.trigger <= 1'b1;
                    l1reg_regs.leftoperand <= MEM_labels_read.rdata[position_by_index*32+31 -: 32];
                    l1reg_regs.rightoperand <= mult_regs.result;
                    line_from_MEM_labels <= MEM_labels_read.rdata;
                    position_by_index <= position_by_index + 1;
                end

                if (l1reg_regs.done)
                begin
                    sub_regs.trigger <= 1'b1;
                    sub_regs.leftoperand <= l1reg_regs.leftoperand;
                    sub_regs.rightoperand <= l1reg_regs.result;
                    from_modify_to_output.we <= 1'b1;
                    from_modify_to_output.wdata <= l1reg_regs.result;
                end

                // if (mult_regs.done)
                // begin
                //     sub_regs.trigger <= 1'b1;
                //     sub_regs.leftoperand <= MEM_labels_read.rdata[position_by_index*32+31 -: 32];
                //     sub_regs.rightoperand <= mult_regs.result;
                //     line_from_MEM_labels <= MEM_labels_read.rdata;
                //     from_modify_to_output.we <= 1'b1;
                //     from_modify_to_output.wdata <= mult_regs.result;
                //     position_by_index <= position_by_index + 1;
                // end

                if (sub_regs.done)
                begin
                    MEM_labels_write.we <= 1'b1;
                    MEM_labels_write.waddr <= MEM_labels_read_load_offset + (offset_by_index_write[15:4]);
                    offset_by_index_write <= offset_by_index_write + 1;

                    for (int i = 0; i < 16; i++)
                    begin
                        MEM_labels_write.wdata[i*32+31 -: 32] <= (i == write_position_by_index) ? sub_regs.result : line_from_MEM_labels[i*32+31 -: 32];
                    end
                    write_position_by_index <= write_position_by_index + 1;

                    write_num_performed_iterations <= write_num_performed_iterations + 1;
                    if (write_num_performed_iterations == num_iterations-1)
                    begin
                        op_done <= 1'b1;
                        modify_state <= STATE_IDLE;
                    end
                end
            end

        endcase

        if (reset)
        begin
            modify_state <= STATE_IDLE;
        end
    end

endmodule // glm_modify