`include "pipearch_common.vh"

module glm_update
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [6],

    fifobram_interface.fifo_read FIFO_samplesforward,
    fifobram_interface.fifo_read FIFO_gradient,
    fifobram_interface.bram_readwrite MEM_model,
    fifobram_interface.fifo_write FIFO_modelforward
);
    
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_GRADIENT_GET,
        STATE_GRADIENT_RECEIVE,
        STATE_MAIN
    } t_updatestate;
    t_updatestate update_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_iterations;
    logic [15:0] MEM_model_offset;
    logic [15:0] MEM_model_length;
    logic write_to_model_forward;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_lines_multiplied_requested;
    logic [15:0] num_lines_multiplied;
    logic [15:0] num_lines_multiplied_final;
    logic [15:0] num_lines_subtracted;
    logic [15:0] num_performed_iterations;
    logic [15:0] num_finished_iterations;

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    logic multiply_trigger;
    logic [31:0] multiply_scalar;
    logic [511:0] multiply_vector;
    logic multiply_valid;
    logic [511:0] multiply_result;
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
        .resetn(!reset),
        .trigger(multiply_trigger),
        .scalar(multiply_scalar),
        .vector(multiply_vector),
        .result_valid(multiply_valid),
        .result(multiply_result)
    );
    always_ff @(posedge clk)
    begin
        multiply_trigger <= 1'b0;
        if (FIFO_samplesforward.rvalid && update_state == STATE_MAIN)
        begin
            multiply_trigger <= 1'b1;
            multiply_scalar <= FIFO_gradient.rdata;
            multiply_vector <= FIFO_samplesforward.rdata;
        end
    end

    logic subtract_trigger;
    logic [511:0] subtract_vector1;
    logic [511:0] subtract_vector2;
    logic subtract_valid;
    logic [511:0] subtract_result;
    float_vector_subtract
    #(
        .VALUES_PER_LINE(16)
    )
    subtract
    (
        .clk,
        .resetn(!reset),
        .trigger(subtract_trigger),
        .vector1(subtract_vector1),
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
            subtract_vector1 <= MEM_model.rdata;
            subtract_vector2 <= multiply_result;
        end
    end

    logic [31:0] debug_MEM_model_wdata [16];
    logic [31:0] debug_multiply_result [16];
    always_comb
    begin
        for (int i = 0; i < 16; i++)
        begin
            debug_MEM_model_wdata[i] = MEM_model.wdata[i*32+31 -: 32];
            debug_multiply_result[i] = multiply_result[i*32+31 -: 32];
        end
    end

    always_ff @(posedge clk)
    begin
        FIFO_gradient.re <= 1'b0;
        FIFO_samplesforward.re <= 1'b0;
        MEM_model.re <= 1'b0;
        MEM_model.we <= 1'b0;
        FIFO_modelforward.we <= 1'b0;
        op_done <= 1'b0;

        case(update_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    MEM_model_offset <= regs[3][15:0];
                    MEM_model_length <= regs[3][31:16];
                    write_to_model_forward <= regs[4][31];
                    num_iterations <= regs[4][15:0];
                    // *************************************************************************
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

                if (num_lines_multiplied_requested == 0 && !FIFO_samplesforward.empty && !FIFO_gradient.empty && !FIFO_modelforward.almostfull)
                begin
                    FIFO_gradient.re <= 1'b1;
                    FIFO_samplesforward.re <= 1'b1;
                    num_lines_multiplied_requested <= num_lines_multiplied_requested + 1;
                end
                else if (num_lines_multiplied_requested > 0 && num_lines_multiplied_requested < MEM_model_length && !FIFO_samplesforward.empty && !FIFO_modelforward.almostfull)
                begin
                    FIFO_samplesforward.re <= 1'b1;
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
                    MEM_model.re <= 1'b1;
                    MEM_model.raddr <= MEM_model_offset + num_lines_multiplied_final;
                    num_lines_multiplied_final <= num_lines_multiplied_final + 1;
                    if (num_lines_multiplied_final == MEM_model_length-1)
                    begin
                        num_lines_multiplied_final <= 0;
                    end
                end

                if (write_to_model_forward)
                begin
                    FIFO_modelforward.we <= subtract_valid;
                    FIFO_modelforward.wdata <= subtract_result;
                end

                if (subtract_valid)
                begin
                    MEM_model.we <= 1'b1;
                    MEM_model.waddr <= MEM_model_offset + num_lines_subtracted;
                    MEM_model.wdata <= subtract_result;
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