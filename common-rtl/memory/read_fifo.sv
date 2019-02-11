`include "pipearch_common.vh"

module read_fifo
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,

    fifobram_interface.fifo_read fifo_access,
    internal_interface.commonread_source outfrom_read
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ
    } t_readstate;
    t_readstate read_state;

    logic[15:0] fifo_read_length;
    logic [15:0] num_read_lines;

    always_ff @(posedge clk)
    begin
        outfrom_read.rvalid <= fifo_access.rvalid;
        outfrom_read.rdata <= fifo_access.rdata;
        fifo_access.re <= 1'b0;

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
                        fifo_read_length <= configreg[31:16];
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
                    if (num_read_lines < fifo_read_length && !fifo_access.empty && !outfrom_read.almostfull)
                    begin
                        fifo_access.re <= 1'b1;
                    end

                    if (fifo_access.rvalid)
                    begin
                        num_read_lines <= num_read_lines + 1;
                        if (num_read_lines == fifo_read_length-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                    end
                end
            endcase
        end
    end

endmodule // read_fifo