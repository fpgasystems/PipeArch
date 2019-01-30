`include "pipearch_common.vh"

module pipearch_dot
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs0,
    input logic [31:0] regs1,

    fifobram_interface.fifo_read samples_input,
    fifobram_interface.bram_read modelMem_input,
    fifobram_interface.fifo_read modelForward_input,

    fifobram_interface.fifo_write dot_output
);
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_dotstate;
    t_dotstate dot_state;

    logic [15:0] num_lines_to_process;
    logic model_input_select;
    logic [15:0] modelMem_load_offset;
    logic [15:0] num_requested_lines;
    logic [15:0] num_processed_lines;


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
    assign dot_trigger = samples_input.rvalid;
    assign dot_left = samples_input.rdata;
    assign dot_right = (model_input_select == 1'b0) ? modelMem_input.rdata : modelForward_input.rdata;
    assign dot_output.we = dot_done;
    assign dot_output.wdata = dot_result;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            dot_state <= STATE_IDLE;
            num_lines_to_process <= 16'b0;
            model_input_select <= 1'b0;
            modelMem_load_offset <= 16'b0; 
            num_requested_lines <= 16'b0;
            num_processed_lines <= 16'b0;
            op_done <= 1'b0;
        end
        else
        begin

            samples_input.re <= 1'b0;
            modelMem_input.re <= 1'b0;
            modelForward_input.re <= 1'b0;
            op_done <= 1'b0;
            case (dot_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        dot_state <= STATE_READ;
                        num_lines_to_process <= regs0[15:0];
                        model_input_select <= regs0[16];
                        modelMem_load_offset <= regs1[15:0];
                        num_requested_lines <= 32'b0;
                        num_processed_lines <= 32'b0;
                    end
                end

                STATE_READ:
                begin
                    if (model_input_select == 1'b0)
                    begin
                        if (!samples_input.empty)
                        begin
                            samples_input.re <= 1'b1;
                            modelMem_input.re <= 1'b1;
                            modelMem_input.raddr <= modelMem_load_offset + num_requested_lines;
                            num_requested_lines <= num_requested_lines + 1;
                        end
                    end
                    else
                    begin
                        if (!samples_input.empty && !modelForward_input.empty)
                        begin
                            samples_input.re <= 1'b1;
                            modelForward_input.re <= 1'b1;
                            num_requested_lines <= num_requested_lines + 1;
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
                        op_done <= 1'b1;
                        dot_state <= STATE_IDLE;
                    end
                end

            endcase
        end
    end

endmodule // pipearch_dot