`include "pipearch_common.vh"

module write_bram
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,

    internal_interface.commonwrite_source into_write,
    fifobram_interface.bram_write memory_access
);
    assign into_write.almostfull = 1'b0;

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ
    } t_readstate;
    t_readstate receive_state;

    bram_access_properties memory_store;
    logic [15:0] num_received_lines;

    always_ff @(posedge clk)
    begin
        memory_access.we <= 1'b0;
        memory_access.wdata <= into_write.wdata;

        if (reset)
        begin
            receive_state <= STATE_IDLE;
        end
        else
        begin
            case (receive_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        memory_store.offset <= configreg[15:0];
                        memory_store.length <= configreg[31:16];
                        num_received_lines <= 16'b0;
                        if (configreg[31:16] == 16'b0)
                        begin
                            receive_state <= STATE_IDLE;
                        end
                        else
                        begin
                            receive_state <= STATE_READ;
                        end
                    end
                end

                STATE_READ:
                begin
                    if (into_write.we && num_received_lines < memory_store.length)
                    begin
                        memory_access.we <= 1'b1;
                        memory_access.waddr <= memory_store.offset + num_received_lines;
                        num_received_lines <= num_received_lines + 1;
                        if (num_received_lines == memory_store.length-1)
                        begin
                            receive_state <= STATE_IDLE;
                        end
                    end
                end
            endcase
        end
    end

endmodule // write_bram