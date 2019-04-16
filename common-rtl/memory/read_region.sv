`include "pipearch_common.vh"

module read_region
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,
    input logic [15:0] iterations,

    fifobram_interface.read region_access,
    internal_interface.commonread_source outfrom_read
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_BRAM_READ,
        STATE_FIFO_READ
    } t_readstate;
    t_readstate read_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    access_properties memory_read;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [LOG2_ACCESS_SIZE-1:0] num_requested_lines;
    logic [15:0] num_performed_iterations;

    always_ff @(posedge clk)
    begin
        outfrom_read.rvalid <= region_access.rvalid;
        outfrom_read.rdata <= region_access.rdata;
        region_access.re <= 1'b0;

        case (read_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    memory_read.write_bram <= configreg[30];
                    memory_read.write_fifo <= configreg[31];
                    memory_read.offset <= configreg[13:0];
                    memory_read.length <= configreg[29:16];
                    memory_read.iterations <= iterations;
                    memory_read.keep_count_along_iterations <= configreg[15];
                    // *************************************************************************
                    num_requested_lines <= 0;
                    num_performed_iterations <= 0;
                    if (configreg[29:16] == 16'b0)
                    begin
                        read_state <= STATE_IDLE;
                    end
                    else if (configreg[31])
                    begin
                        read_state <= STATE_FIFO_READ;
                    end
                    else
                    begin
                        read_state <= STATE_BRAM_READ;
                    end
                end
            end

            STATE_BRAM_READ:
            begin
                if (num_requested_lines < memory_read.length && !outfrom_read.almostfull)
                begin
                    region_access.re <= 1'b1;
                    region_access.rfifobram <= 2'b01;
                    region_access.raddr <= memory_read.offset + num_requested_lines;
                    num_requested_lines <= num_requested_lines + 1;

                    if (num_requested_lines == memory_read.length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == memory_read.iterations-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_requested_lines <= 0;
                            if (memory_read.keep_count_along_iterations)
                            begin
                                memory_read.offset <= memory_read.offset + memory_read.length;
                            end
                        end
                    end
                end
            end

            STATE_FIFO_READ:
            begin
                if (num_requested_lines < memory_read.length && !region_access.empty && !outfrom_read.almostfull)
                begin
                    region_access.re <= 1'b1;
                    region_access.rfifobram <= 2'b10;
                    num_requested_lines <= num_requested_lines + 1;

                    if (num_requested_lines == memory_read.length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == memory_read.iterations-1)
                        begin
                            read_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_requested_lines <= 0;
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            read_state <= STATE_IDLE;
        end
    end

endmodule // read_bram