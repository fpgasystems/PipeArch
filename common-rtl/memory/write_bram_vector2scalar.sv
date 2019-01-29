`include "pipearch_common.vh"

module write_bram_vector2scalar
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,
    // configreg [15:0] write offset
    // configreg [31:16] write length

    internal_interface.commonwrite_source into_write,
    fifobram_interface.bram_write memory_access
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_WRITE
    } t_readstate;
    t_readstate receive_state;

    bram_access_properties memory_store;
    logic [15:0] num_received_lines;
    logic [31:0] num_written_words;
    logic [4:0] position;

    always_ff @(negedge clk)
    begin
        if (reset)
        begin
            into_write.almostfull <= 1'b0;
        end
        else
        begin
            case (receive_state)
                STATE_IDLE:
                begin
                    into_write.almostfull <= 1'b0;
                end

                STATE_READ:
                begin
                    if (into_write.we)
                    begin
                        into_write.almostfull <= 1'b1;
                    end
                    else 
                    begin
                        into_write.almostfull <= 1'b0;
                    end
                end

                STATE_WRITE:
                begin
                    into_write.almostfull <= 1'b1;
                end
            endcase
        end
    end

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            receive_state <= STATE_IDLE;
            memory_access.we <= 1'b0;
            
        end
        else
        begin
            memory_access.we <= 1'b0;
            
            case (receive_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        memory_store.offset <= configreg[15:0];
                        memory_store.length <= configreg[31:16];
                        num_received_lines <= 16'b0;
                        num_written_words <= 32'b0;
                        position <= 4'b0;
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
                    if (into_write.we)
                    begin
                        receive_state <= STATE_WRITE;
                    end
                end

                STATE_WRITE:
                begin
                    memory_access.we <= 1'b1;
                    memory_access.waddr <= memory_store.offset + (num_received_lines << 4) + position;
                    memory_access.wdata <= into_write.wdata[position*32+31 -: 32];
                    position <= position + 1;

                    if (position == 15)
                    begin
                        num_received_lines <= num_received_lines + 1;
                        if (num_received_lines == memory_store.length-1)
                        begin
                            receive_state <= STATE_IDLE;
                        end
                        else
                        begin
                            receive_state <= STATE_READ;
                        end
                    end

                end
            endcase
        end
    end

endmodule // write_bram_vector2scalar