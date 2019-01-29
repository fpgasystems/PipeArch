`include "pipearch_common.vh"

module pipearch_scalar_subtract
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
    logic [31:0] num_processed_lines;

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_state;
    t_state state;

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(6)) leftoperand_fifo_access();
    fifo
    #(.WIDTH(32), .LOG2_DEPTH(6)
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

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(6)) rightoperand_fifo_access();
    fifo
    #(.WIDTH(32), .LOG2_DEPTH(6)
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

    assign leftoperand_fifo_access.re = !leftoperand_fifo_access.empty && !rightoperand_fifo_access.empty && !result.almostfull;
    assign rightoperand_fifo_access.re = !leftoperand_fifo_access.empty && !rightoperand_fifo_access.empty && !result.almostfull;

    localparam OP_LATENCY = 2;
    logic [OP_LATENCY-1:0] localop_status = 0;
    logic localop_trigger;
    logic localop_done;
    logic [31:0] op_result;
    always_ff @(posedge clk)
    begin
        localop_status[0] <= localop_trigger;
        for (int i = 1; i < OP_LATENCY; i++)
        begin
            localop_status[i] <= localop_status[i-1];
        end
        localop_done <= localop_status[OP_LATENCY-1];
    end
    fp_subtract_arria10
    subtract
    (
        .clk,
        .areset(reset),
        .a(leftoperand_fifo_access.rdata),
        .b(rightoperand_fifo_access.rdata),
        .q(op_result)
    );
    assign localop_trigger = leftoperand_fifo_access.rvalid && rightoperand_fifo_access.rvalid;
    assign result.we = localop_done;
    assign result.wdata = op_result;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            state <= STATE_IDLE;
        end
        else
        begin
            op_done <= 1'b0;
            case (state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        state <= STATE_READ;
                        num_lines_to_process <= regs0 >> 16;
                        num_processed_lines <= 32'b0;
                    end
                end

                STATE_READ:
                begin
                    if (localop_trigger)
                    begin
                        num_processed_lines <= num_processed_lines + 1;
                        if (num_processed_lines == num_lines_to_process-1)
                        begin
                            state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    if (localop_done)
                    begin
                        op_done <= 1'b1;
                        state <= STATE_IDLE;
                    end
                end
            endcase
        end
    end

endmodule // pipearch_scalar_subtract