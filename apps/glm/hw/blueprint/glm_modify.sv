`include "pipearch_common.vh"

module glm_modify
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [7],

    fifobram_interface.bram_readwrite MEM_labels,
    fifobram_interface.fifo_read FIFO_dot,

    fifobram_interface.fifo_write FIFO_gradient
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
    logic [15:0] MEM_labels_load_offset;
    logic [1:0] model_type;
    logic algorithm_type;
    logic [31:0] step_size;
    logic [31:0] lambda;
    logic [511:0] lineFromLabelsMem;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_performed_iterations;
    logic [15:0] write_num_performed_iterations;

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

    localparam SUBTRACT_LATENCY = 2;
    logic [SUBTRACT_LATENCY-1:0] sub_status = 0;
    fp_compute_regs sub_regs;
    always_ff @(posedge clk)
    begin
        sub_status[0] <= sub_regs.trigger;
        for (int i = 1; i < SUBTRACT_LATENCY; i++)
        begin
            sub_status[i] <= sub_status[i-1];
        end
        sub_regs.done <= sub_status[SUBTRACT_LATENCY-1];
    end
    fp_subtract_arria10
    subtract
    (
        .clk,
        .areset(reset),
        .a(sub_regs.leftoperand),
        .b(sub_regs.rightoperand),
        .q(sub_regs.result)
    );

    localparam MULTIPLY_LATENCY = 2;
    logic [MULTIPLY_LATENCY-1:0] mult_status = 0;
    fp_compute_regs mult_regs;
    always_ff @(posedge clk)
    begin
        mult_status[0] <= mult_regs.trigger;
        for (int i = 1; i < MULTIPLY_LATENCY; i++)
        begin
            mult_status[i] <= mult_status[i-1];
        end
        mult_regs.done <= mult_status[MULTIPLY_LATENCY-1];
    end
    fp_mult_arria10
    multiply
    (
        .clk,
        .areset(reset),
        .a(mult_regs.leftoperand),
        .b(mult_regs.rightoperand),
        .q(mult_regs.result)
    );

    always_ff @(posedge clk)
    begin
        sub_regs.trigger <= 1'b0;
        mult_regs.trigger <= 1'b0;
        FIFO_dot.re <= 1'b0;
        MEM_labels.re <= 1'b0;
        MEM_labels.we <= 1'b0;
        FIFO_gradient.we <= 1'b0;
        op_done <= 1'b0;

        case (modify_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    offset_by_index <= regs[0][15:0];
                    offset_by_index_write <= regs[0][15:0];
                    position_by_index <= regs[0][3:0];
                    write_position_by_index <= regs[0][3:0];
                    num_iterations <= regs[3][31:16];
                    MEM_labels_load_offset <= regs[3][15:0];
                    model_type <= regs[4][1:0];
                    algorithm_type <= regs[4][2];
                    step_size <= regs[5];
                    lambda <= regs[6];
                    // *************************************************************************
                    num_performed_iterations <= 0;
                    write_num_performed_iterations <= 0;
                    modify_state <= (regs[4][2] == 1'b0) ? STATE_SGD_MAIN : STATE_SCD_MAIN;
                end
            end

            STATE_SGD_MAIN:
            begin
                if (!FIFO_dot.empty && num_performed_iterations < num_iterations)
                begin
                    FIFO_dot.re <= 1'b1;
                    MEM_labels.re <= 1'b1;
                    MEM_labels.raddr <= MEM_labels_load_offset + (offset_by_index[15:4]);
                    num_performed_iterations <= num_performed_iterations + 1;
                    offset_by_index <= offset_by_index + 1;
                end

                if (FIFO_dot.rvalid && MEM_labels.rvalid)
                begin
                    sub_regs.trigger <= 1'b1;
                    sub_regs.leftoperand <= FIFO_dot.rdata;
                    sub_regs.rightoperand <= MEM_labels.rdata[position_by_index*32+31 -: 32];
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
                    FIFO_gradient.we <= 1'b1;
                    FIFO_gradient.wdata <= mult_regs.result;
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
                if (!FIFO_dot.empty && num_performed_iterations < num_iterations)
                begin
                    FIFO_dot.re <= 1'b1;
                    num_performed_iterations <= num_performed_iterations + 1;
                end

                if (FIFO_dot.rvalid)
                begin
                    mult_regs.trigger <= 1'b1;
                    mult_regs.leftoperand <= step_size;
                    mult_regs.rightoperand <= FIFO_dot.rdata;;
                end

                if (mult_status[0])
                begin
                    MEM_labels.re <= 1'b1;
                    MEM_labels.raddr <= MEM_labels_load_offset + (offset_by_index[15:4]);
                    offset_by_index <= offset_by_index + 1;
                end

                if (mult_regs.done)
                begin
                    sub_regs.trigger <= 1'b1;
                    sub_regs.leftoperand <= MEM_labels.rdata[position_by_index*32+31 -: 32];
                    sub_regs.rightoperand <= mult_regs.result;
                    lineFromLabelsMem <= MEM_labels.rdata;
                    FIFO_gradient.we <= 1'b1;
                    FIFO_gradient.wdata <= mult_regs.result;
                    position_by_index <= position_by_index + 1;
                end

                if (sub_regs.done)
                begin
                    MEM_labels.we <= 1'b1;
                    MEM_labels.waddr <= MEM_labels_load_offset + (offset_by_index_write[15:4]);
                    offset_by_index_write <= offset_by_index_write + 1;

                    for (int i = 0; i < 16; i++)
                    begin
                        MEM_labels.wdata[i*32+31 -: 32] <= (i == write_position_by_index) ? sub_regs.result : lineFromLabelsMem[i*32+31 -: 32];
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