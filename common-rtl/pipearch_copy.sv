`include "pipearch_common.vh"

module pipearch_copy
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [2],
    
    fifobram_interface.read REGION_read,
    fifobram_interface.write REGION_write
);

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read[2]();

    internal_interface #(.WIDTH(512)) from_REGION_read();
    read_region
    read_REGION_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[0]),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[0].read),
        .region_access(REGION_read),
        .outfrom_read(from_REGION_read.commonread_source)
    );

    write_region
    write_REGION_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[1]),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[1].read),
        .into_write(from_REGION_read.commonwrite_source),
        .region_access(REGION_write)
    );

    assign from_REGION_read.we = from_REGION_read.rvalid;
    assign from_REGION_read.wdata = from_REGION_read.rdata;

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

        case(copy_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_copy <= regs[0][29:16];
                    // *************************************************************************
                    num_copied_lines <= 0;
                    copy_state <= STATE_COPY;
                end
            end

            STATE_COPY:
            begin
                if (from_REGION_read.we)
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

        if (reset)
        begin
            copy_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_copy