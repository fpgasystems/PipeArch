`include "pipearch_common.vh"

module glm_dot
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [6],

    fifobram_interface.fifo_read FIFO_input,
    fifobram_interface.bram_read MEM_labels,
    fifobram_interface.bram_read MEM_model,
    fifobram_interface.fifo_read FIFO_modelforward,
    fifobram_interface.fifo_write FIFO_dot
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_RESET,
        STATE_READ,
        STATE_DONE
    } t_dotstate;
    t_dotstate dot_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_iterations;
    logic read_from_modelforward;
    logic perform_label_subtraction;
    logic [15:0] num_lines_to_process;
    logic [15:0] MEM_model_load_offset;
    logic [15:0] MEM_labels_load_offset;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_requested_lines;
    logic [15:0] num_read_lines;
    logic [15:0] num_processed_lines;
    logic [15:0] num_performed_iterations;

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
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
        .reset(reset),
        .trigger(subtract_trigger),
        .vector1(subtract_vector1),
        .vector2(subtract_vector2),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );
    always_ff @(posedge clk)
    begin
        subtract_trigger <= 1'b0;
        if (MEM_labels.rvalid && perform_label_subtraction && dot_state == STATE_READ)
        begin
            subtract_trigger <= 1'b1;
            subtract_vector1 <= (read_from_modelforward) ? FIFO_modelforward.rdata : MEM_model.rdata;
            subtract_vector2 <= MEM_labels.rdata;
        end
    end

    logic FIFO_input_rvalid_d [3:0];
    logic [511:0] FIFO_input_rdata_d [3:0];
    always_ff @(posedge clk)
    begin
        FIFO_input_rvalid_d[0] <= FIFO_input.rvalid;
        FIFO_input_rvalid_d[1] <= FIFO_input_rvalid_d[0];
        FIFO_input_rvalid_d[2] <= FIFO_input_rvalid_d[1];
        FIFO_input_rvalid_d[3] <= FIFO_input_rvalid_d[2];
        FIFO_input_rdata_d[0] <= FIFO_input.rdata;
        FIFO_input_rdata_d[1] <= FIFO_input_rdata_d[0];
        FIFO_input_rdata_d[2] <= FIFO_input_rdata_d[1];
        FIFO_input_rdata_d[3] <= FIFO_input_rdata_d[2];
    end

    logic dot_trigger;
    logic [511:0] dot_left;
    logic [511:0] dot_right;
    logic dot_done;
    logic [31:0] dot_result;
    hybrid_dot_product
    #(
        .LOG2_VALUES_PER_LINE(4)
    )
    dot_compute
    (
        .clk,
        .resetn(!reset),
        .trigger(dot_trigger),
        .accumulation_count(32'(num_lines_to_process)),
        .vector1(dot_left),
        .vector2(dot_right),
        .result_valid(dot_done),
        .result(dot_result)
    );
    always_ff @(posedge clk)
    begin
        if (perform_label_subtraction)
        begin
            dot_trigger <= FIFO_input_rvalid_d[3];
            dot_left <= subtract_result;
            dot_right <= FIFO_input_rdata_d[3];
        end
        else
        begin
            dot_trigger <= FIFO_input.rvalid;
            dot_left <= (read_from_modelforward) ? FIFO_modelforward.rdata : MEM_model.rdata;
            dot_right <= FIFO_input.rdata;
        end
    end
    assign FIFO_dot.we = dot_done;
    assign FIFO_dot.wdata = dot_result;

    always_ff @(posedge clk)
    begin
        
        FIFO_input.re <= 1'b0;
        MEM_model.re <= 1'b0;
        MEM_labels.re <= 1'b0;
        FIFO_modelforward.re <= 1'b0;
        op_done <= 1'b0;

        case (dot_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    read_from_modelforward <= regs[3][16];
                    perform_label_subtraction <= regs[3][17];
                    num_lines_to_process <= regs[3][15:0];
                    MEM_model_load_offset <= regs[4][15:0];
                    MEM_labels_load_offset <= regs[4][31:16];
                    num_iterations <= regs[5][15:0];
                    // *************************************************************************
                    num_performed_iterations <= 0;
                    num_requested_lines <= 0;
                    num_read_lines <= 0;
                    num_processed_lines <= 0;
                    dot_state <= STATE_READ;
                end
            end

            STATE_READ:
            begin
                if (!FIFO_input.empty && num_requested_lines < num_lines_to_process)
                begin
                    if (perform_label_subtraction)
                    begin
                        if (!read_from_modelforward)
                        begin
                            FIFO_input.re <= 1'b1;
                            MEM_labels.re <= 1'b1;
                            MEM_labels.raddr <= MEM_labels_load_offset + num_requested_lines;
                            MEM_model.re <= 1'b1;
                            MEM_model.raddr <= MEM_model_load_offset + num_requested_lines;
                            num_requested_lines <= num_requested_lines + 1;
                        end
                        else if (!FIFO_modelforward.empty)
                        begin
                            FIFO_input.re <= 1'b1;
                            MEM_labels.re <= 1'b1;
                            MEM_labels.raddr <= MEM_labels_load_offset + num_requested_lines;
                            FIFO_modelforward.re <= 1'b1;
                            num_requested_lines <= num_requested_lines + 1;
                        end
                    end
                    else
                    begin
                        if (!read_from_modelforward)
                        begin
                            FIFO_input.re <= 1'b1;
                            MEM_model.re <= 1'b1;
                            MEM_model.raddr <= MEM_model_load_offset + num_requested_lines;
                            num_requested_lines <= num_requested_lines + 1;
                        end
                        else if (!FIFO_modelforward.empty)
                        begin
                            FIFO_input.re <= 1'b1;
                            FIFO_modelforward.re <= 1'b1;
                            num_requested_lines <= num_requested_lines + 1;
                        end
                    end
                end

                if (FIFO_input.rvalid)
                begin
                    num_read_lines <= num_read_lines + 1;
                    if (num_read_lines == num_lines_to_process-1)
                    begin
                        num_read_lines <= 0;
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations < num_iterations-1)
                        begin
                            num_requested_lines <= 32'b0;
                        end
                    end
                end

                if (dot_trigger)
                begin
                    num_processed_lines <= num_processed_lines + 1;
                    if (num_processed_lines == num_lines_to_process-1)
                    begin
                        if (num_performed_iterations == num_iterations)
                        begin
                            dot_state <= STATE_IDLE;
                            op_done <= 1'b1;
                        end
                        else
                        begin
                            num_processed_lines <= 32'b0;
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            dot_state <= STATE_IDLE;
        end
    end

endmodule // glm_dot