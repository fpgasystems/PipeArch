`include "pipearch_common.vh"

module glm_delta
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [5],

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
    logic do_sigmoid;

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
    logic sigmoid_trigger;
    logic sigmoid_valid;
    logic [CLDATA_WIDTH-1:0] sigmoid_result;
    float_vector_sigmoid
    #(
        .VALUES_PER_LINE(16)
    )
    sigmoid
    (
        .clk, .reset,
        .trigger(sigmoid_trigger),
        .vector(FIFO_REGION_left_read.rdata),
        .result_valid(sigmoid_valid),
        .result(sigmoid_result)
    );
    always_ff @(posedge clk)
    begin
        FIFO_REGION_left_read.re <= 1'b0;
        if (!FIFO_REGION_left_read.empty && !FIFO_sigmoid_interface.almostfull)
        begin
            FIFO_REGION_left_read.re <= 1'b1;
        end
    end
    assign sigmoid_trigger = do_sigmoid ? FIFO_REGION_left_read.rvalid : 1'b0;

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_sigmoid_interface();
    fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6))
    FIFO_region (
        .clk, .reset,
        .access(FIFO_sigmoid_interface.fifo_source)
    );
    assign FIFO_sigmoid_interface.we = do_sigmoid ? sigmoid_valid : FIFO_REGION_left_read.rvalid;
    assign FIFO_sigmoid_interface.wdata = do_sigmoid ? sigmoid_result : FIFO_REGION_left_read.rdata;

    logic subtract_trigger;
    logic subtract_valid;
    logic [CLDATA_WIDTH-1:0] subtract_result;
    float_vector_subtract
    #(
        .VALUES_PER_LINE(16)
    )
    subtract
    (
        .clk, .reset,
        .trigger(subtract_trigger),
        .vector1(FIFO_sigmoid_interface.rdata),
        .vector2(FIFO_REGION_right_read.rdata),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );
    always_ff @(posedge clk)
    begin
        FIFO_sigmoid_interface.re <= 1'b0;
        FIFO_REGION_right_read.re <= 1'b0;
        if (!FIFO_sigmoid_interface.empty && !FIFO_REGION_right_read.empty && !from_subtract_to_output.almostfull)
        begin
            FIFO_sigmoid_interface.re <= 1'b1;
            FIFO_REGION_right_read.re <= 1'b1;
        end
    end
    assign subtract_trigger = FIFO_sigmoid_interface.rvalid && FIFO_REGION_right_read.rvalid;

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
                    do_sigmoid <= regs[4][0];
                    // *************************************************************************
                    num_processed_lines <= 0;
                    num_performed_iterations <= 0;
                    delta_state <= STATE_PROCESS;
                    read_trigger <= 1'b1;
                end
            end

            STATE_PROCESS:
            begin
                if (subtract_valid)
                begin
                    num_processed_lines <= num_processed_lines + 1;
                    if (num_processed_lines == num_lines_to_process-1)
                    begin
                        num_processed_lines <= 0;
                        num_performed_iterations <= num_performed_iterations + 1;
                        if (num_performed_iterations == num_iterations-1)
                        begin
                            op_done <= 1'b1;
                            delta_state <= STATE_IDLE;
                        end
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