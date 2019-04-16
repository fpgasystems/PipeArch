`include "pipearch_common.vh"

module write_region
(
    input logic clk,
    input logic reset,

    input logic op_start,
    input logic [31:0] configreg,
    input logic [15:0] iterations,

    internal_interface.commonwrite_source into_write,
    fifobram_interface.write region_access
);
    assign into_write.almostfull = region_access.almostfull;

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_WRITE
    } t_readstate;
    t_readstate receive_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    access_properties memory_store;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [LOG2_ACCESS_SIZE-1:0] num_received_lines;
    logic [15:0] num_performed_iterations;

    always_ff @(posedge clk)
    begin
        region_access.we <= 1'b0;
        region_access.wdata <= into_write.wdata;
        region_access.wfifobram <= {memory_store.write_fifo, memory_store.write_bram};

        case (receive_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    memory_store.write_bram <= configreg[30];
                    memory_store.write_fifo <= configreg[31];
                    memory_store.offset <= configreg[13:0];
                    memory_store.length <= configreg[29:16];
                    memory_store.iterations <= iterations;
                    memory_store.keep_count_along_iterations <= configreg[15];
                    // *************************************************************************
                    num_received_lines <= 0;
                    num_performed_iterations <= 0;
                    if (configreg[29:16] == 16'b0)
                    begin
                        receive_state <= STATE_IDLE;
                    end
                    else
                    begin
                        receive_state <= STATE_WRITE;
                    end
                end
            end

            STATE_WRITE:
            begin
                if (into_write.we && num_received_lines < memory_store.length)
                begin
                    region_access.we <= 1'b1;
                    region_access.waddr <= memory_store.offset + num_received_lines;
                    num_received_lines <= num_received_lines + 1;
                    if (num_received_lines == memory_store.length-1)
                    begin
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == memory_store.iterations-1)
                        begin
                            receive_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_received_lines <= 0;
                            if (memory_store.keep_count_along_iterations)
                            begin
                                memory_store.offset <= memory_store.offset + memory_store.length;
                            end
                        end
                    end
                end
            end
        endcase

        if (reset)
        begin
            receive_state <= STATE_IDLE;
        end
    end
endmodule // write_region