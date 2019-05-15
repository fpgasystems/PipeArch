`include "pipearch_common.vh"

module pipearch_writeforward
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [1],
    
    fifobram_interface.writeforward REGION_writeforward,
    fifobram_interface.write REGION_write
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_FORWARD
    } t_forwardstate;
    t_forwardstate forward_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_lines_to_forward;
    logic [15:0] num_iterations;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_forwarded_lines;
    logic [15:0] num_performed_iterations;

    always_ff @(posedge clk)
    begin
        REGION_write.we <= 1'b0;
        REGION_write.waddr <= REGION_writeforward.waddr;
        REGION_write.wdata <= REGION_writeforward.wdata;
        REGION_write.wfifobram <= REGION_writeforward.wfifobram;
        op_done <= 1'b0;

        case(forward_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_forward <= regs[0][15:0];
                    num_iterations <= regs[0][31:16];
                    // *************************************************************************
                    num_forwarded_lines <= 0;
                    num_performed_iterations <= 0;
                    forward_state <= STATE_FORWARD;
                end
            end

            STATE_FORWARD:
            begin
                if (REGION_writeforward.we)
                begin
                    REGION_write.we <= 1'b1;
                    num_forwarded_lines <= num_forwarded_lines + 1;
                    if (num_forwarded_lines == num_lines_to_forward-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            forward_state <= STATE_IDLE;
                            op_done <= 1'b1;
                        end
                        else
                        begin
                            num_forwarded_lines <= 0;
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            forward_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_copy