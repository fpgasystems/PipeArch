`include "pipearch_common.vh"

module glm_dot
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [4],

    fifobram_interface.read MEM_props_left,
    fifobram_interface.read MEM_props_right,
    fifobram_interface.read REGION_left_read,
    fifobram_interface.read REGION_right_read,
    fifobram_interface.write REGION_dot_write
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_TRIGGER,
        STATE_PROCESS
    } t_dotstate;
    t_dotstate dot_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_lines_to_process;
    logic [15:0] num_iterations;
    logic [31:0] REGION_left_read_accessproperties;
    logic [31:0] REGION_right_read_accessproperties;
    logic [31:0] output_accessproperties;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic read_trigger;
    logic [15:0] num_processed_lines;
    logic [15:0] num_performed_iterations;

    // *************************************************************************
    //
    //   Read Channels
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_left_read();
    read_region2fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6))
    read_REGION_left_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_left_read_accessproperties),
        .iterations(num_iterations),
        .props_access(MEM_props_left),
        .region_access(REGION_left_read),
        .fifo_access(FIFO_REGION_left_read.read_source)
    );

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_right_read();
    read_region2fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6))
    read_REGION_right_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_right_read_accessproperties),
        .iterations(num_iterations),
        .props_access(MEM_props_right),
        .region_access(REGION_right_read),
        .fifo_access(FIFO_REGION_right_read.read_source)
    );

    // *************************************************************************
    //
    //   Write Channels
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read();

    internal_interface #(.WIDTH(32)) from_dot_to_output();
    write_region
    write_REGION_dot_write (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(output_accessproperties),
        .iterations(num_iterations),
        .into_write(from_dot_to_output.commonwrite_source),
        .props_access(dummy_accessprops_read.read),
        .region_access(REGION_dot_write)
    );

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    logic dot_trigger;
    logic dot_done;
    logic [31:0] dot_result;
    hybrid_dot_product
    #(.LOG2_VALUES_PER_LINE(4))
    dot_compute
    (
        .clk,
        .resetn(!reset),
        .trigger(dot_trigger),
        .accumulation_count(32'(num_lines_to_process)),
        .vector1(FIFO_REGION_left_read.rdata),
        .vector2(FIFO_REGION_right_read.rdata),
        .result_valid(dot_done),
        .result(dot_result)
    );
    always_ff @(posedge clk)
    begin
        FIFO_REGION_left_read.re <= 1'b0;
        FIFO_REGION_right_read.re <= 1'b0;
        if (!FIFO_REGION_left_read.empty && !FIFO_REGION_right_read.empty && !from_dot_to_output.almostfull)
        begin
            FIFO_REGION_left_read.re <= 1'b1;
            FIFO_REGION_right_read.re <= 1'b1;
        end
    end
    assign dot_trigger = FIFO_REGION_left_read.rvalid && FIFO_REGION_right_read.rvalid;

    assign from_dot_to_output.we = dot_done;
    assign from_dot_to_output.wdata = dot_result;

    always_ff @(posedge clk)
    begin
        read_trigger <= 1'b0;
        op_done <= 1'b0;

        case (dot_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_process <= regs[0][15:0];
                    num_iterations <= regs[0][31:16];
                    REGION_left_read_accessproperties <= regs[1];
                    REGION_right_read_accessproperties <= regs[2];
                    output_accessproperties <= regs[3];
                    // *************************************************************************
                    num_processed_lines <= 0;
                    num_performed_iterations <= 0;
                    dot_state <= STATE_PROCESS;
                    read_trigger <= 1'b1;
                end
            end

            STATE_PROCESS:
            begin
                if (dot_trigger)
                begin
                    num_processed_lines <= num_processed_lines + 1;
                    if (num_processed_lines == num_lines_to_process-1)
                    begin
                        num_processed_lines <= 0;
                    end
                end

                if (dot_done)
                begin
                    num_performed_iterations <= num_performed_iterations + 1;
                    if (num_performed_iterations == num_iterations-1)
                    begin
                        op_done <= 1'b1;
                        dot_state <= STATE_IDLE;
                    end
                end
            end
        endcase

        if (reset)
        begin
            dot_state <= STATE_IDLE;
        end
    end

endmodule // glm_dot