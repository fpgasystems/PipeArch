`include "pipearch_common.vh"

module glm_l2reg
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [6],
    
    fifobram_interface.read REGION_modelold_read,
    fifobram_interface.read REGION_modelnew_read,
    fifobram_interface.write REGION_modelforward_write,
    fifobram_interface.write REGION_modelnew_write
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_PROCESS,
        STATE_COPY
    } t_l2state;
    t_l2state l2state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic [15:0] num_lines_to_process;
    logic [31:0] lambda;
    logic [31:0] REGION_modelold_read_accessproperties;
    logic [31:0] REGION_modelnew_read_accessproperties;
    logic [31:0] REGION_modelforward_write_accessproperties;
    logic [31:0] REGION_modelnew_write_accessproperties;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic read_trigger;
    logic [15:0] num_processed_lines;

    // *************************************************************************
    //
    //   Read Channels
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read[4]();

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_modelold_read();
    read_region2fifo
    read_REGION_modelold_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_modelold_read_accessproperties),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[0].read),
        .region_access(REGION_modelold_read),
        .fifo_access(FIFO_REGION_modelold_read.read_source)
    );

    logic FIFO_REGION_modelnew_read_re;
    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_modelnew_read();
    read_region2fifo
    read_REGION_modelnew_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_modelnew_read_accessproperties),
        .iterations(16'd1),
        .props_access(dummy_accessprops_read[1].read),
        .region_access(REGION_modelnew_read),
        .fifo_access(FIFO_REGION_modelnew_read.read_source)
    );

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    logic multiply_valid;
    logic [CLDATA_WIDTH-1:0] multiply_result;
    logic [CLDATA_WIDTH-1:0] multiply_result_1d;
    float_scalar_vector_mult
    #(
        .VALUES_PER_LINE(16)
    )
    multiply
    (
        .clk,
        .reset(reset),
        .trigger(FIFO_REGION_modelold_read.rvalid),
        .scalar(lambda),
        .vector(FIFO_REGION_modelold_read.rdata),
        .result_valid(multiply_valid),
        .result(multiply_result)
    );

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
        .trigger(FIFO_REGION_modelnew_read.rvalid),
        .vector1(FIFO_REGION_modelnew_read.rdata),
        .vector2(multiply_result_1d),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );
    assign FIFO_REGION_modelnew_read.re = (l2state == STATE_COPY) ? FIFO_REGION_modelnew_read_re : multiply_valid;

    // *************************************************************************
    //
    //   Write Channels
    //
    // *************************************************************************
    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_subtract_to_modelforward();
    write_region
    write_REGION_modelforward_write (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_modelforward_write_accessproperties),
        .iterations(16'd1),
        .into_write(from_subtract_to_modelforward.commonwrite_source),
        .props_access(dummy_accessprops_read[2].read),
        .region_access(REGION_modelforward_write)
    );
    assign from_subtract_to_modelforward.we = subtract_valid;
    assign from_subtract_to_modelforward.wdata = subtract_result;

    internal_interface #(.WIDTH(CLDATA_WIDTH)) from_subtract_to_model();
    write_region
    write_REGION_modelnew_write (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_modelnew_write_accessproperties),
        .iterations(16'd1),
        .into_write(from_subtract_to_model.commonwrite_source),
        .props_access(dummy_accessprops_read[3].read),
        .region_access(REGION_modelnew_write)
    );
    assign from_subtract_to_model.we = subtract_valid;
    assign from_subtract_to_model.wdata = subtract_result;

    always_ff @(posedge clk)
    begin
        read_trigger <= 1'b0;
        op_done <= 1'b0;

        FIFO_REGION_modelold_read.re <= 1'b0;
        FIFO_REGION_modelnew_read_re <= 1'b0;

        case(l2state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_process <= regs[0][15:0];
                    REGION_modelold_read_accessproperties <= regs[1];
                    REGION_modelnew_read_accessproperties <= regs[2];
                    REGION_modelforward_write_accessproperties <= regs[3];
                    REGION_modelnew_write_accessproperties <= regs[4];
                    lambda <= regs[5];
                    // *************************************************************************
                    read_trigger <= 1'b1;
                    num_processed_lines <= 0;
                    if (regs[1] == 32'b0) begin
                        l2state <= STATE_COPY;
                    end
                    else begin
                        l2state <= STATE_PROCESS;
                    end
                end
            end

            STATE_PROCESS:
            begin
                if (!FIFO_REGION_modelold_read.empty && !FIFO_REGION_modelnew_read.empty &&
                    !from_subtract_to_modelforward.almostfull && !from_subtract_to_model.almostfull)
                begin
                    FIFO_REGION_modelold_read.re <= 1'b1;
                end

                multiply_result_1d <= multiply_result;

                if (subtract_valid)
                begin
                    num_processed_lines <= num_processed_lines + 1;
                    if (num_processed_lines == num_lines_to_process-1)
                    begin
                        l2state <= STATE_IDLE;
                        op_done <= 1'b1;
                    end
                end
            end

            STATE_COPY:
            begin
                if (!FIFO_REGION_modelnew_read.empty && !from_subtract_to_modelforward.almostfull)
                begin
                    FIFO_REGION_modelnew_read_re <= 1'b1;
                end

                multiply_result_1d <= 0;

                if (subtract_valid)
                begin
                    num_processed_lines <= num_processed_lines + 1;
                    if (num_processed_lines == num_lines_to_process-1)
                    begin
                        l2state <= STATE_IDLE;
                        op_done <= 1'b1;
                    end
                end
            end
        endcase

        if (reset)
        begin
            l2state <= STATE_IDLE;
        end
    end

endmodule // glm_l2reg