`include "pipearch_common.vh"

module pipearch_dot
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs0,

    internal_interface.from_commonread left_input,
    internal_interface.from_commonread right_input,
    internal_interface.to_commonwrite result
);

    logic [31:0] num_lines_to_process;
    logic [31:0] num_lines_left;
    logic [31:0] num_lines_right;
    logic [31:0] num_processed_lines;

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_dotstate;
    t_dotstate dot_state;

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(6)) leftoperand_fifo_access();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(6)
    )
    leftoperand_fifo
    (
        .clk,
        .reset,
        .access(leftoperand_fifo_access.fifo_source)
    );
    assign leftoperand_fifo_access.we = left_input.rvalid;
    assign leftoperand_fifo_access.wdata = left_input.rdata;
    assign left_input.almostfull = leftoperand_fifo_access.almostfull;

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(6)) rightoperand_fifo_access();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(6)
    )
    rightoperand_fifo
    (
        .clk,
        .reset,
        .access(rightoperand_fifo_access.fifo_source)
    );
    assign rightoperand_fifo_access.we = right_input.rvalid;
    assign rightoperand_fifo_access.wdata = right_input.rdata;
    assign right_input.almostfull = rightoperand_fifo_access.almostfull;

    logic dot_trigger;
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
        .vector1(leftoperand_fifo_access.rdata),
        .vector2(rightoperand_fifo_access.rdata),
        .result_valid(dot_done),
        .result(dot_result)
    );
    assign dot_trigger = leftoperand_fifo_access.rvalid && rightoperand_fifo_access.rvalid;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            dot_state <= STATE_IDLE;
            num_lines_to_process <= 32'b0;
            num_lines_left <= 32'b0;
            num_lines_right <= 32'b0;
            num_processed_lines <= 32'b0;

            leftoperand_fifo_access.re <= 1'b0;
            rightoperand_fifo_access.re <= 1'b0;
            result.we <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin

            leftoperand_fifo_access.re <= 1'b0;
            rightoperand_fifo_access.re <= 1'b0;
            result.we <= 1'b0;
            op_done <= 1'b0;
            case (dot_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        dot_state <= STATE_READ;
                        num_lines_to_process <= regs0 >> 16;
                        num_lines_left <= 32'b0;
                        num_lines_right <= 32'b0;
                        num_processed_lines <= 32'b0;
                    end
                end

                STATE_READ:
                begin
                    if (!leftoperand_fifo_access.empty && !rightoperand_fifo_access.empty && !result.almostfull)
                    begin
                        leftoperand_fifo_access.re <= 1'b1;
                        rightoperand_fifo_access.re <= 1'b1;
                    end

                    if (leftoperand_fifo_access.rvalid)
                    begin
                        num_lines_left <= num_lines_left + 1;
                    end
                    if (rightoperand_fifo_access.rvalid)
                    begin
                        num_lines_right <= num_lines_right + 1;
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
                        result.we <= 1'b1;
                        result.wdata <= dot_result;
                        dot_state <= STATE_IDLE;
                    end
                end

            endcase
        end
    end

endmodule // pipearch_dot