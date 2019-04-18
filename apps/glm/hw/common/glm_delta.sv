`include "pipearch_common.vh"

module glm_delta
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [4],

    fifobram_interface.read REGION_left_read,
    fifobram_interface.read REGION_right_read,
    fifobram_interface.write REGION_delta_write
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
    } t_deltastate;
    t_deltastate delta_state;

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
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read[3]();

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_left_read();
    read_region2fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6))
    read_REGION_left_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_left_read_accessproperties),
        .iterations(num_iterations),
        .props_access(dummy_accessprops_read[0].read),
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
        .props_access(dummy_accessprops_read[1].read),
        .region_access(REGION_right_read),
        .fifo_access(FIFO_REGION_right_read.read_source)
    );

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    logic subtract_trigger;
    logic subtract_valid;
    logic [CLDATA_WIDTH-1:0] subtract_result;
    float_vector_subtract
    #(
        .VALUES_PER_LINE(16)
    )
    subtract
    (
        .clk,
        .reset(reset),
        .trigger(subtract_trigger),
        .vector1(FIFO_REGION_left_read.rdata),
        .vector2(FIFO_REGION_right_read.rdata),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );
    always_ff @(posedge clk)
    begin
        FIFO_REGION_left_read.re <= 1'b0;
        FIFO_REGION_right_read.re <= 1'b0;
        if (!FIFO_REGION_left_read.empty && !FIFO_REGION_right_read.empty)
        begin
            FIFO_REGION_left_read.re <= 1'b1;
            FIFO_REGION_right_read.re <= 1'b1;
        end
    end
    assign subtract_trigger = FIFO_REGION_left_read.rvalid && FIFO_REGION_right_read.rvalid;

    // *************************************************************************
    //
    //   Write Channels
    //
    // *************************************************************************
    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_subtract_to_output();
    write_region
    write_REGION_delta_write (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(output_accessproperties),
        .iterations(num_iterations),
        .into_write(from_subtract_to_output.commonwrite_source),
        .props_access(dummy_accessprops_read[2].read),
        .region_access(REGION_delta_write)
    );
    assign from_subtract_to_output.we = subtract_valid;
    assign from_subtract_to_output.wdata = subtract_result;

    always_ff @(posedge clk)
    begin
        read_trigger <= 1'b0;
        op_done <= 1'b0;

        case (delta_state)
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
                    delta_state <= STATE_PROCESS;
                    read_trigger <= 1'b1;
                end
            end

            STATE_PROCESS:
            begin
                if (subtract_trigger)
                begin
                    num_processed_lines <= num_processed_lines + 1;
                    if (num_processed_lines == num_lines_to_process-1)
                    begin
                        num_processed_lines <= 0;
                    end
                end

                if (subtract_valid)
                begin
                    num_performed_iterations <= num_performed_iterations + 1;
                    if (num_performed_iterations == num_iterations-1)
                    begin
                        op_done <= 1'b1;
                        delta_state <= STATE_IDLE;
                    end
                end
            end
        endcase

        if (reset)
        begin
            delta_state <= STATE_IDLE;
        end
    end

endmodule // glm_delta