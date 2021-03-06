`include "pipearch_common.vh"

module glm_update
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [6],

    fifobram_interface.read MEM_props_samples,
    fifobram_interface.read MEM_props_model,
    fifobram_interface.read REGION_samples_read,
    fifobram_interface.read REGION_gradient_read,
    fifobram_interface.read MEM_model_read,
    fifobram_interface.write REGION_model_write
);
    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_FETCH_PROPS,
        STATE_RECEIVE_PROPS,
        STATE_MAIN_WITH_PROPS,
        STATE_MAIN_WITH_PROPS_ASYNC,
        STATE_MAIN
    } t_updatestate;
    t_updatestate update_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic enable_async;
    logic [15:0] num_lines_to_process;
    logic [15:0] num_iterations;
    logic [31:0] REGION_samples_read_accessproperties;
    logic [31:0] REGION_gradient_read_accessproperties;
    access_properties MEM_model_read_accessproperties;
    access_properties REGION_model_write_accessproperties;
    logic [17:0] accessprops_raddr;
    logic [17:0] accessprops_waddr;
    logic [CLDATA_WIDTH-1:0] accessprops_data;
    logic [CLDATA_WIDTH-1:0] accessprops_data_1d;
    logic [CLDATA_WIDTH-1:0] accessprops_data_2d;
    logic [3:0] accessprops_read_position;
    logic [LOG2_ACCESS_SIZE-1:0] current_read_offset;
    logic [LOG2_ACCESS_SIZE-1:0] current_read_length;
    logic [3:0] accessprops_write_position;
    logic [LOG2_ACCESS_SIZE-1:0] current_write_offset;
    logic [LOG2_ACCESS_SIZE-1:0] current_write_length;

    assign accessprops_read_position = accessprops_raddr[3:0];
    assign current_read_offset = accessprops_data[accessprops_read_position*32+13 -: 14];
    assign current_read_length = accessprops_data[accessprops_read_position*32+29 -: 14];
    assign accessprops_write_position = accessprops_waddr[3:0];
    assign current_write_offset = accessprops_data_2d[accessprops_write_position*32+13 -: 14];
    assign current_write_length = accessprops_data_2d[accessprops_write_position*32+29 -: 14];

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic read_trigger;
    logic [15:0] num_lines_multiplied_requested;
    logic [15:0] num_lines_subtracted_requested;
    logic [15:0] num_lines_subtracted;
    logic [15:0] num_lines_subtracted_1d;
    logic [15:0] num_multiply_iterations;
    logic [15:0] num_subtract_iterations1;
    logic [15:0] num_subtract_iterations2;

    // *************************************************************************
    //
    //   Read Channels
    //
    // *************************************************************************

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_REGION_samples_read();
    read_region2fifo
    #(.WIDTH(CLDATA_WIDTH))
    read_REGION_samples_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_samples_read_accessproperties),
        .iterations(num_iterations),
        .props_access(MEM_props_samples),
        .region_access(REGION_samples_read),
        .fifo_access(FIFO_REGION_samples_read.read_source)
    );

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(1)) dummy_accessprops_read[1]();
    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(6)) FIFO_REGION_gradient_read();
    read_region2fifo
    #(.WIDTH(32))
    read_REGION_gradient_read (
        .clk, .reset,
        .op_start(read_trigger),
        .configreg(REGION_gradient_read_accessproperties),
        .iterations(num_iterations),
        .props_access(dummy_accessprops_read[0].read),
        .region_access(REGION_gradient_read),
        .fifo_access(FIFO_REGION_gradient_read.read_source)
    );

    // *************************************************************************
    //
    //   Computation
    //
    // *************************************************************************
    logic gradient_read;
    logic multiply_valid;
    logic [31:0] gradient;
    logic [31:0] multiply_scalar;
    logic [CLDATA_WIDTH-1:0] multiply_result;
    float_scalar_vector_mult
    #(
        .VALUES_PER_LINE(16)
    )
    multiply
    (
        .clk,
        .reset(reset),
        .trigger(FIFO_REGION_samples_read.rvalid),
        .scalar(multiply_scalar),
        .vector(FIFO_REGION_samples_read.rdata),
        .result_valid(multiply_valid),
        .result(multiply_result)
    );
    assign multiply_scalar = FIFO_REGION_gradient_read.rvalid ? FIFO_REGION_gradient_read.rdata : gradient;

    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(6)) FIFO_multiply();
    fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(5))
    FIFO_input (
        .clk, .reset,
        .access(FIFO_multiply.fifo_source)
    );
    assign FIFO_multiply.we = multiply_valid;
    assign FIFO_multiply.wdata = multiply_result;

    always_ff @(posedge clk)
    begin
        FIFO_REGION_gradient_read.re <= 1'b0;
        FIFO_REGION_samples_read.re <= 1'b0;

        if (num_lines_multiplied_requested < num_lines_to_process && (!FIFO_REGION_gradient_read.empty || gradient_read == 1'b1) &&!FIFO_REGION_samples_read.empty && !FIFO_multiply.almostfull)
        begin
            if (num_lines_multiplied_requested == 0)
            begin
                FIFO_REGION_gradient_read.re <= 1'b1;
                gradient_read <= 1'b1;
            end
            FIFO_REGION_samples_read.re <= 1'b1;
            num_lines_multiplied_requested <= num_lines_multiplied_requested + 1;
            if (num_lines_multiplied_requested == num_lines_to_process-1)
            begin
                num_multiply_iterations <= num_multiply_iterations + 1;
                if (num_multiply_iterations < num_iterations-1)
                begin
                    gradient_read <= 1'b0;
                    num_lines_multiplied_requested <= 0;
                end
            end
        end

        if (FIFO_REGION_gradient_read.rvalid)
        begin
            gradient <= FIFO_REGION_gradient_read.rdata;
        end

        if (update_state == STATE_IDLE)
        begin
            gradient_read <= 1'b0;
            num_lines_multiplied_requested <= 0;
            num_multiply_iterations <= 0;
        end
    end

    logic subtract_valid;
    logic subtract_valid_1d;
    logic [CLDATA_WIDTH-1:0] subtract_result;
    float_vector_subtract
    #(
        .VALUES_PER_LINE(16)
    )
    subtract
    (
        .clk,
        .reset(reset),
        .trigger(FIFO_multiply.rvalid),
        .vector1(MEM_model_read.rdata),
        .vector2(FIFO_multiply.rdata),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );

    logic [31:0] monitor_multiply_result [16];
    logic [31:0] monitor_subtract_result [16];
    always_ff @(posedge clk)
    begin
        for (int i = 0; i < 16; i++)
        begin
            monitor_multiply_result[i] <= multiply_result[i*32+31 -: 32];
            monitor_subtract_result[i] <= subtract_result[i*32+31 -: 32];
        end
    end

    always_ff @(posedge clk)
    begin
        accessprops_data_1d <= accessprops_data;
        accessprops_data_2d <= accessprops_data_1d;

        subtract_valid_1d <= subtract_valid;
        num_lines_subtracted_1d <= num_lines_subtracted;

        read_trigger <= 1'b0;
        MEM_props_model.re <= 1'b0;
        FIFO_multiply.re <= 1'b0;
        MEM_model_read.re <= 1'b0;
        REGION_model_write.we <= 1'b0;
        REGION_model_write.wfifobram <= {REGION_model_write_accessproperties.write_fifo, REGION_model_write_accessproperties.write_bram};
        op_done <= 1'b0;

        case(update_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    num_lines_to_process <= regs[0][15:0];
                    num_iterations <= regs[0][31:16];
                    REGION_samples_read_accessproperties <= regs[1];
                    REGION_gradient_read_accessproperties <= regs[2];
                    MEM_model_read_accessproperties <= regs[3];
                    REGION_model_write_accessproperties <= regs[4];
                    enable_async <= regs[5][0];
                    accessprops_raddr <= {regs[3][13:0], 4'b0000};
                    accessprops_waddr <= {regs[4][13:0], 4'b0000};
                    // *************************************************************************
                    read_trigger <= 1'b1;
                    num_lines_subtracted_requested <= 0;
                    num_lines_subtracted <= 0;
                    num_subtract_iterations1 <= 0;
                    num_subtract_iterations2 <= 0;
                    if (regs[3][14])
                    begin
                        update_state <= STATE_FETCH_PROPS;
                    end
                    else
                    begin
                        update_state <= STATE_MAIN;
                    end
                end
            end

            STATE_FETCH_PROPS:
            begin
                MEM_props_model.re <= 1'b1;
                MEM_props_model.rfifobram <= 2'b01;
                MEM_props_model.raddr <= accessprops_raddr >> 4;
                update_state <= STATE_RECEIVE_PROPS;
            end

            STATE_RECEIVE_PROPS:
            begin
                if (MEM_props_model.rvalid)
                begin
                    accessprops_data <= MEM_props_model.rdata;
                    update_state <= enable_async ? STATE_MAIN_WITH_PROPS_ASYNC : STATE_MAIN_WITH_PROPS;
                    num_lines_subtracted_requested <= 0;
                end
            end

            STATE_MAIN_WITH_PROPS:
            begin
                if (num_lines_subtracted_requested < num_lines_to_process && !FIFO_multiply.empty && !REGION_model_write.almostfull)
                begin
                    FIFO_multiply.re <= 1'b1;
                    MEM_model_read.re <= 1'b1;
                    MEM_model_read.rfifobram <= 2'b01;
                    MEM_model_read.raddr <= current_read_offset + num_lines_subtracted_requested;
                    num_lines_subtracted_requested <= num_lines_subtracted_requested + 1;
                end

                if (subtract_valid_1d)
                begin
                    if (num_lines_subtracted_1d == num_lines_to_process-1)
                    begin
                        num_subtract_iterations1 <= num_subtract_iterations1 + 1;
                        if (num_subtract_iterations1 < num_iterations-1)
                        begin
                            num_lines_subtracted_requested <= 0;
                        end
                    end
                end

                if (subtract_valid)
                begin
                    REGION_model_write.we <= 1'b1;
                    REGION_model_write.waddr <= current_read_offset + num_lines_subtracted;
                    REGION_model_write.wdata <= subtract_result;
                    num_lines_subtracted <= num_lines_subtracted + 1;
                    if (num_lines_subtracted == num_lines_to_process-1)
                    begin
                        num_subtract_iterations2 <= num_subtract_iterations2 + 1;
                        if (num_subtract_iterations2 == num_iterations-1)
                        begin
                            op_done <= 1'b1;
                            update_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_lines_subtracted <= 0;
                            if (MEM_model_read_accessproperties.keep_count_along_iterations)
                            begin
                                accessprops_raddr <= accessprops_raddr + MEM_model_read_accessproperties.length;
                            end
                            if (num_subtract_iterations2[3:0] == 4'd15)
                            begin
                                update_state <= STATE_FETCH_PROPS;
                            end
                        end
                    end
                end
            end

            STATE_MAIN_WITH_PROPS_ASYNC:
            begin
                if (num_lines_subtracted_requested < num_lines_to_process && !FIFO_multiply.empty && !REGION_model_write.almostfull)
                begin
                    FIFO_multiply.re <= 1'b1;
                    MEM_model_read.re <= 1'b1;
                    MEM_model_read.rfifobram <= 2'b01;
                    MEM_model_read.raddr <= current_read_offset + num_lines_subtracted_requested;
                    num_lines_subtracted_requested <= num_lines_subtracted_requested + 1;
                    if (num_lines_subtracted_requested == num_lines_to_process-1)
                    begin
                        num_subtract_iterations1 <= num_subtract_iterations1 + 1;
                        // if (num_subtract_iterations1 < num_iterations-1 && num_subtract_iterations1[3:0] < 4'd15)
                        if (num_subtract_iterations1 < num_iterations-1)
                        begin
                            num_lines_subtracted_requested <= 0;
                            if (MEM_model_read_accessproperties.keep_count_along_iterations)
                            begin
                                accessprops_raddr <= accessprops_raddr + MEM_model_read_accessproperties.length;
                            end
                            if (num_subtract_iterations1[3:0] == 4'd15)
                            begin
                                update_state <= STATE_FETCH_PROPS;
                            end
                        end
                    end
                end

                // if (subtract_valid)
                // begin
                //     REGION_model_write.we <= 1'b1;
                //     REGION_model_write.waddr <= current_write_offset + num_lines_subtracted;
                //     REGION_model_write.wdata <= subtract_result;
                //     num_lines_subtracted <= num_lines_subtracted + 1;
                //     if (num_lines_subtracted == num_lines_to_process-1)
                //     begin
                //         num_subtract_iterations2 <= num_subtract_iterations2 + 1;
                //         if (num_subtract_iterations2 == num_iterations-1)
                //         begin
                //             op_done <= 1'b1;
                //             update_state <= STATE_IDLE;
                //         end
                //         else
                //         begin
                //             num_lines_subtracted <= 0;
                //             if (REGION_model_write_accessproperties.keep_count_along_iterations)
                //             begin
                //                 accessprops_waddr <= accessprops_waddr + REGION_model_write_accessproperties.length;
                //             end
                //             if (num_subtract_iterations2[3:0] == 4'd15)
                //             begin
                //                 update_state <= STATE_FETCH_PROPS;
                //             end
                //         end
                //     end
                // end
            end

            STATE_MAIN:
            begin
                if (num_lines_subtracted_requested < num_lines_to_process && !FIFO_multiply.empty && !REGION_model_write.almostfull)
                begin
                    FIFO_multiply.re <= 1'b1;
                    MEM_model_read.re <= 1'b1;
                    MEM_model_read.rfifobram <= 2'b01;
                    MEM_model_read.raddr <= MEM_model_read_accessproperties.offset + num_lines_subtracted_requested;
                    num_lines_subtracted_requested <= num_lines_subtracted_requested + 1;
                end

                if (subtract_valid_1d)
                begin
                    if (num_lines_subtracted_1d == num_lines_to_process-1)
                    begin
                        num_subtract_iterations1 <= num_subtract_iterations1 + 1;
                        if (num_subtract_iterations1 < num_iterations-1)
                        begin
                            num_lines_subtracted_requested <= 0;
                        end
                    end
                end

                if (subtract_valid)
                begin
                    REGION_model_write.we <= 1'b1;
                    REGION_model_write.waddr <= REGION_model_write_accessproperties.offset + num_lines_subtracted;
                    REGION_model_write.wdata <= subtract_result;
                    REGION_model_write.wfifobram <= {REGION_model_write_accessproperties.write_fifo, REGION_model_write_accessproperties.write_bram};
                    num_lines_subtracted <= num_lines_subtracted + 1;
                    if (num_lines_subtracted == num_lines_to_process-1)
                    begin
                        num_subtract_iterations2 <= num_subtract_iterations2 + 1;
                        if (num_subtract_iterations2 == num_iterations-1)
                        begin
                            op_done <= 1'b1;
                            update_state <= STATE_IDLE;
                        end
                        else
                        begin
                            num_lines_subtracted <= 0;
                        end
                    end
                end
            end
        endcase

        if (enable_async)
        begin
            if (subtract_valid)
            begin
                REGION_model_write.we <= 1'b1;
                REGION_model_write.waddr <= current_write_offset + num_lines_subtracted;
                REGION_model_write.wdata <= subtract_result;
                num_lines_subtracted <= num_lines_subtracted + 1;
                if (num_lines_subtracted == num_lines_to_process-1)
                begin
                    num_subtract_iterations2 <= num_subtract_iterations2 + 1;
                    if (num_subtract_iterations2 == num_iterations-1)
                    begin
                        op_done <= 1'b1;
                        update_state <= STATE_IDLE;
                    end
                    else
                    begin
                        num_lines_subtracted <= 0;
                        if (REGION_model_write_accessproperties.keep_count_along_iterations)
                        begin
                            accessprops_waddr <= accessprops_waddr + REGION_model_write_accessproperties.length;
                        end
                    end
                end
            end
        end

        if (reset)
        begin
            update_state <= STATE_IDLE;
        end
    end

endmodule // glm_update