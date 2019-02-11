`include "pipearch_common.vh"

module read_bram
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,

    fifobram_interface.bram_read memory_access,
    internal_interface.commonread_source outfrom_read
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ
    } t_readstate;
    t_readstate read_state;

    bram_access_properties memory_read;
    logic [15:0] num_read_lines;

    always_ff @(posedge clk)
    begin
        outfrom_read.rvalid <= memory_access.rvalid;
        outfrom_read.rdata <= memory_access.rdata;
        memory_access.re <= 1'b0;

        if (reset)
        begin
            read_state <= STATE_IDLE;
        end
        else
        begin
            case (read_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        memory_read.offset <= configreg[15:0];
                        memory_read.length <= configreg[31:16];
                        num_read_lines <= 32'b0;
                        if (configreg[31:16] == 16'b0)
                        begin
                            read_state <= STATE_IDLE;
                        end
                        else
                        begin
                            read_state <= STATE_READ;
                        end
                    end
                end

                STATE_READ:
                begin
                    if (num_read_lines < memory_read.length && !outfrom_read.almostfull)
                    begin
                        memory_access.re <= 1'b1;
                        memory_access.raddr <= memory_read.offset + num_read_lines;
                        num_read_lines <= num_read_lines + 1;
                        if (num_read_lines == memory_read.length-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                    end
                end
            endcase
        end
    end

endmodule // read_bram