`include "pipearch_common.vh"

module glm_dot
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],

    fifobram_interface.fifo_read FIFO_input,
    fifobram_interface.bram_read MEM_labels,
    fifobram_interface.bram_read MEM_model,
    fifobram_interface.fifo_read FIFO_modelforward,
    fifobram_interface.fifo_write FIFO_dot
);
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_dotstate;
    t_dotstate dot_state;

    logic read_from_modelforward;
    logic perform_label_subtraction;
    logic [15:0] num_lines_to_process;
    logic [15:0] MEM_model_load_offset;
    logic [15:0] MEM_labels_load_offset;
    logic [15:0] num_requested_lines;
    logic [15:0] num_processed_lines;


    logic subtract_trigger;
    logic [511:0] subtract_vector1;
    logic [511:0] subtract_vector2;
    logic subtract_valid;
    logic [511:0] subtract_result;
    logic [1:0] subtract_trigger_d;
    always_ff @(posedge clk)
    begin
        subtract_trigger_d[0] <= subtract_trigger;
        subtract_trigger_d[1] <= subtract_trigger_d[0];
    end
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
    assign subtract_trigger = MEM_labels.rvalid;
    assign subtract_vector1 = (read_from_modelforward) ? FIFO_modelforward.rdata : MEM_model.rdata;
    assign subtract_vector2 = MEM_labels.rdata;


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
    assign dot_trigger = FIFO_input.rvalid;
    always_comb
    begin
        if (perform_label_subtraction)
        begin
            dot_left = subtract_result;
        end
        else
        begin
            dot_left = (read_from_modelforward) ? FIFO_modelforward.rdata : MEM_model.rdata;
        end
    end
    assign dot_right = FIFO_input.rdata;
    assign FIFO_dot.we = dot_done;
    assign FIFO_dot.wdata = dot_result;
    assign op_done = dot_done;


    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            dot_state <= STATE_IDLE;
            read_from_modelforward <= 1'b0;
            perform_label_subtraction <= 1'b0;
            num_lines_to_process <= 16'b0;
            MEM_model_load_offset <= 16'b0; 
            num_requested_lines <= 16'b0;
            num_processed_lines <= 16'b0;
        end
        else
        begin

            FIFO_input.re <= 1'b0;
            MEM_model.re <= 1'b0;
            MEM_labels.re <= 1'b0;
            FIFO_modelforward.re <= 1'b0;
            case (dot_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        dot_state <= STATE_READ;

                        read_from_modelforward <= regs[3][16];
                        perform_label_subtraction <= regs[3][17];
                        num_lines_to_process <= regs[3][15:0];
                        MEM_model_load_offset <= regs[4][15:0];
                        MEM_labels_load_offset <= regs[4][31:16];
                        num_requested_lines <= 32'b0;
                        num_processed_lines <= 32'b0;
                    end
                end

                STATE_READ:
                begin

                    if (!FIFO_input.empty)
                    begin
                        if (perform_label_subtraction)
                        begin
                            if (num_requested_lines < num_lines_to_process)
                            begin
                                if (!read_from_modelforward)
                                begin
                                    MEM_labels.re <= 1'b1;
                                    MEM_labels.raddr <= MEM_labels_load_offset + num_requested_lines;
                                    MEM_model.re <= 1'b1;
                                    MEM_model.raddr <= MEM_model_load_offset + num_requested_lines;
                                    num_requested_lines <= num_requested_lines + 1;
                                end
                                else if (!FIFO_modelforward.empty)
                                begin
                                    MEM_labels.re <= 1'b1;
                                    MEM_labels.raddr <= MEM_labels_load_offset + num_requested_lines;
                                    FIFO_modelforward.re <= 1'b1;
                                    num_requested_lines <= num_requested_lines + 1;
                                end
                            end
                            if (subtract_trigger_d[2])
                            begin
                                FIFO_input.re <= 1'b1;
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

                    if (dot_trigger)
                    begin
                        num_processed_lines <= num_processed_lines + 1;
                        if (num_processed_lines == num_lines_to_process-1)
                        begin
                            dot_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    if (dot_done == 1'b1)
                    begin
                        dot_state <= STATE_IDLE;
                    end
                end

            endcase
        end
    end

endmodule // glm_dot