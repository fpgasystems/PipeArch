`include "pipearch_common.vh"

module glm_update
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [5],

    fifobram_interface.read REGION_samples_read,
    fifobram_interface.read REGION_gradient_read,
    fifobram_interface.read MEM_model_read,
    fifobram_interface.write REGION_model_write
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_MAIN
    } t_updatestate;
    t_updatestate update_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_lines_to_process;
    logic [15:0] num_iterations;
    logic [31:0] REGION_samples_read_accessproperties;
    logic [31:0] REGION_gradient_read_accessproperties;
    logic [15:0] MEM_model_offset;
    logic [13:0] MEM_model_length;
    logic [31:0] REGION_model_write_accessproperties;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic read_trigger;
    logic [15:0] num_lines_multiplied_requested;
    logic [15:0] num_lines_multiplied;
    logic [15:0] num_lines_multiplied_final;
    logic [15:0] num_lines_subtracted;
    logic [15:0] num_performed_iterations;
    logic [15:0] num_finished_iterations;

    // *************************************************************************
    //
    //   Read Channels
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read[2]();

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_samples_read();
    read_region2fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6))
    read_REGION_samples_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_samples_read_accessproperties),
        .iterations(num_iterations),
        .props_access(dummy_accessprops_read[0].read),
        .region_access(REGION_samples_read),
        .fifo_access(FIFO_REGION_samples_read.read_source)
    );

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(6)) FIFO_REGION_gradient_read();
    read_region2fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6))
    read_REGION_gradient_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_gradient_read_accessproperties),
        .iterations(num_iterations),
        .props_access(dummy_accessprops_read[1].read),
        .region_access(REGION_gradient_read),
        .fifo_access(FIFO_REGION_gradient_read.read_source)
    );

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    logic multiply_trigger;
    logic [31:0] multiply_scalar;
    logic [CLDATA_WIDTH-1:0] multiply_vector;
    logic multiply_valid;
    logic [CLDATA_WIDTH-1:0] multiply_result;
    logic [1:0] multiply_trigger_d;
    always_ff @(posedge clk)
    begin
        multiply_trigger_d[0] <= multiply_trigger;
        multiply_trigger_d[1] <= multiply_trigger_d[0];
    end
    float_scalar_vector_mult
    #(
        .VALUES_PER_LINE(16)
    )
    multiply
    (
        .clk,
        .reset(reset),
        .trigger(multiply_trigger),
        .scalar(multiply_scalar),
        .vector(multiply_vector),
        .result_valid(multiply_valid),
        .result(multiply_result)
    );
    always_ff @(posedge clk)
    begin
        multiply_trigger <= 1'b0;
        if (FIFO_REGION_samples_read.rvalid && update_state == STATE_MAIN)
        begin
            multiply_trigger <= 1'b1;
            multiply_scalar <= FIFO_REGION_gradient_read.rdata;
            multiply_vector <= FIFO_REGION_samples_read.rdata;
        end
    end

    logic subtract_trigger;
    logic [CLDATA_WIDTH-1:0] subtract_vector2;
    logic subtract_valid;
    logic [CLDATA_WIDTH-1:0] subtract_result;
    float_vector_subtract
    #(
        .VALUES_PER_LINE(16)
    )
    subtract
    (
        .clk,
        .reset(reset),
        .trigger(subtract_trigger),
        .vector1(MEM_model_read.rdata),
        .vector2(subtract_vector2),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );
    always_ff @(posedge clk)
    begin
        subtract_trigger <= 1'b0;
        if (multiply_valid && update_state == STATE_MAIN)
        begin
            subtract_trigger <= 1'b1;
            subtract_vector2 <= multiply_result;
        end
    end


    always_ff @(posedge clk)
    begin
        read_trigger <= 1'b0;
        FIFO_REGION_gradient_read.re <= 1'b0;
        FIFO_REGION_samples_read.re <= 1'b0;
        MEM_model_read.re <= 1'b0;
        MEM_model_read.rfifobram <= 2'b01;
        REGION_model_write.we <= 1'b0;
        op_done <= 1'b0;

        case(update_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_process <= regs[0][15:0];
                    num_iterations <= regs[0][31:16];
                    REGION_samples_read_accessproperties <= regs[1];
                    REGION_gradient_read_accessproperties <= regs[2];
                    MEM_model_offset <= regs[3][15:0];
                    MEM_model_length <= regs[3][29:16];
                    REGION_model_write_accessproperties <= regs[4];
                    // *************************************************************************
                    read_trigger <= 1'b1;
                    num_performed_iterations <= 0;
                    num_finished_iterations <= 0;
                    num_lines_multiplied_requested <= 16'b0;
                    num_lines_multiplied <= 16'b0;
                    num_lines_multiplied_final <= 16'b0;
                    num_lines_subtracted <= 16'b0;
                    update_state <= STATE_MAIN;
                end
            end

            STATE_MAIN:
            begin
                if (num_lines_multiplied_requested == 0 && !FIFO_REGION_samples_read.empty && !FIFO_REGION_gradient_read.empty && !REGION_model_write.almostfull)
                begin
                    FIFO_REGION_gradient_read.re <= 1'b1;
                    FIFO_REGION_samples_read.re <= 1'b1;
                    num_lines_multiplied_requested <= num_lines_multiplied_requested + 1;
                end
                else if (num_lines_multiplied_requested > 0 && num_lines_multiplied_requested < MEM_model_length && !FIFO_REGION_samples_read.empty && !REGION_model_write.almostfull)
                begin
                    FIFO_REGION_samples_read.re <= 1'b1;
                    num_lines_multiplied_requested <= num_lines_multiplied_requested + 1;
                end

                if (multiply_valid)
                begin
                    num_lines_multiplied <= num_lines_multiplied + 1;
                    if (num_lines_multiplied == MEM_model_length-1)
                    begin
                        num_lines_multiplied <= 0;
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations < num_iterations-1)
                        begin
                            num_lines_multiplied_requested <= 16'b0;
                        end
                    end
                end

                if (multiply_trigger_d[1])
                begin
                    MEM_model_read.re <= 1'b1;
                    MEM_model_read.raddr <= MEM_model_offset + num_lines_multiplied_final;
                    num_lines_multiplied_final <= num_lines_multiplied_final + 1;
                    if (num_lines_multiplied_final == MEM_model_length-1)
                    begin
                        num_lines_multiplied_final <= 0;
                    end
                end

                if (subtract_valid)
                begin
                    REGION_model_write.we <= 1'b1;
                    REGION_model_write.waddr <= REGION_model_write_accessproperties[15:0] + num_lines_subtracted;
                    REGION_model_write.wdata <= subtract_result;
                    REGION_model_write.wfifobram <= REGION_model_write_accessproperties[31:30];
                    num_lines_subtracted <= num_lines_subtracted + 1;
                    if (num_lines_subtracted == MEM_model_length-1)
                    begin
                        num_lines_subtracted <= 0;
                        num_finished_iterations <= num_finished_iterations + 1;
                        if (num_finished_iterations == num_iterations-1)
                        begin
                            op_done <= 1'b1;
                            update_state <= STATE_IDLE;
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            update_state <= STATE_IDLE;
        end
    end

endmodule // glm_update