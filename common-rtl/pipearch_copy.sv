`include "pipearch_common.vh"

module pipearch_copy
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [3],
    
    fifobram_interface.read REGION_read,
    fifobram_interface.write REGION0_write,
    fifobram_interface.write REGION1_write,
    fifobram_interface.write REGION2_write
);

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [31:0] REGION_read_accessproperties;
    logic [31:0] REGION0_write_accessproperties;
    logic [31:0] REGION1_write_accessproperties;
    logic [31:0] REGION2_write_accessproperties;
    logic [15:0] num_lines_to_copy;

    // *************************************************************************
    //
    //   Load Channels
    //
    // *************************************************************************
    logic write_trigger;
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read[4]();

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_REGION_read();
    read_region
    read_REGION_inst (
        .clk, .reset,
        .op_start(write_trigger),
        .configreg(REGION_read_accessproperties),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[0].read),
        .region_access(REGION_read),
        .outfrom_read(from_REGION_read.commonread_source)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_read_to_REGION0();
    write_region
    write_REGION0_inst (
        .clk, .reset,
        .op_start(write_trigger),
        .configreg(REGION0_write_accessproperties),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[1].read),
        .into_write(from_read_to_REGION0.commonwrite_source),
        .region_access(REGION0_write)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_read_to_REGION1();
    write_region
    write_REGION1_inst (
        .clk, .reset,
        .op_start(write_trigger),
        .configreg(REGION1_write_accessproperties),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[2].read),
        .into_write(from_read_to_REGION1.commonwrite_source),
        .region_access(REGION1_write)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_read_to_REGION2();
    write_region
    write_REGION2_inst (
        .clk, .reset,
        .op_start(write_trigger),
        .configreg(REGION2_write_accessproperties),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[3].read),
        .into_write(from_read_to_REGION2.commonwrite_source),
        .region_access(REGION2_write)
    );

    always_ff @(posedge clk)
    begin
        from_read_to_REGION0.we <= from_REGION_read.rvalid;
        from_read_to_REGION0.wdata <= from_REGION_read.rdata;
        from_read_to_REGION1.we <= from_REGION_read.rvalid;
        from_read_to_REGION1.wdata <= from_REGION_read.rdata;
        from_read_to_REGION2.we <= from_REGION_read.rvalid;
        from_read_to_REGION2.wdata <= from_REGION_read.rdata;
        from_REGION_read.almostfull <=  from_read_to_REGION0.almostfull |
                                        from_read_to_REGION1.almostfull |
                                        from_read_to_REGION2.almostfull;
    end
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
    //   Counter
    //
    // *************************************************************************
    logic [15:0] num_copied_lines;

    always_ff @(posedge clk)
    begin
        write_trigger <= 1'b0;
        op_done <= 1'b0;

        case(copy_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_copy <= regs[0][29:16];
                    REGION_read_accessproperties <= regs[0];
                    REGION0_write_accessproperties <= regs[2][0] ? regs[1] : 0;
                    REGION1_write_accessproperties <= regs[2][1] ? regs[1] : 0;
                    REGION2_write_accessproperties <= regs[2][2] ? regs[1] : 0;
                    // *************************************************************************
                    write_trigger <= 1'b1;
                    num_copied_lines <= 0;
                    copy_state <= STATE_COPY;
                end
            end

            STATE_COPY:
            begin
                if (from_REGION_read.rvalid)
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