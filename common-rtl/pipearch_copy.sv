`include "pipearch_common.vh"

module pipearch_copy
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [5],
    
    fifobram_interface.bram_read MEM_read,
    fifobram_interface.bram_write MEM_write
);

    internal_interface #(.WIDTH(512)) from_MEM_read();
    read_bram
    read_MEM_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[3]),
        .memory_access(MEM_read),
        .outfrom_read(from_MEM_read.commonread_source)
    );

    write_bram
    write_MEM_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[4]),
        .into_write(from_MEM_read.commonwrite_source),
        .memory_access(MEM_write)
    );

    assign from_MEM_read.we = from_MEM_read.rvalid;
    assign from_MEM_read.wdata = from_MEM_read.rdata;

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_COPY
    } t_copystate;
    t_copystate copy_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_lines_to_copy;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_copied_lines;

    always_ff @(posedge clk)
    begin
        op_done <= 1'b0;

        if (reset)
        begin
            copy_state <= STATE_IDLE;
        end
        else
        begin
            case(copy_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        // *************************************************************************
                        num_lines_to_copy <= regs[3][31:16];
                        // *************************************************************************
                        num_copied_lines <= 0;
                        copy_state <= STATE_COPY;
                    end
                end

                STATE_COPY:
                begin
                    if (from_MEM_read.we)
                    begin
                        num_copied_lines <= num_copied_lines + 1;
                        if (num_copied_lines == num_lines_to_copy-1)
                        begin
                            copy_state <= STATE_IDLE;
                            op_done <= 1'b1;
                        end
                    end
                end
            endcase
        end

    end

endmodule // pipearch_copy