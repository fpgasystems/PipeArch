`include "pipearch_common.vh"

module glm_modify
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],

    fifobram_interface.bram_readwrite MEM_labels,
    fifobram_interface.fifo_read FIFO_dot,

    fifobram_interface.fifo_write FIFO_gradient
);

    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_SGD_MAIN,
        STATE_SCD_MAIN
    } t_modifystate;
    t_modifystate modify_state;

    typedef struct packed {
        logic       trigger;
        logic[31:0] leftoperand;
        logic[31:0] rightoperand;
        logic[31:0] result;
        logic       done;
    } fp_compute_regs;

    logic [15:0] offsetByIndex;
    logic [3:0] positionByIndex;
    logic [15:0] MEM_labels_load_offset;
    // logic [15:0] MEM_labels_store_offset;
    logic [1:0] model_type;
    logic algorithm_type;
    logic [31:0] step_size;
    logic [31:0] lambda;
    logic [31:0] scalarFromLabelsMem;
    logic [511:0] lineFromLabelsMem;

    localparam SUBTRACT_LATENCY = 2;
    logic [SUBTRACT_LATENCY-1:0] sub_status = 0;
    fp_compute_regs sub_regs;
    always_ff @(posedge clk)
    begin
        sub_status[0] <= sub_regs.trigger;
        for (int i = 1; i < SUBTRACT_LATENCY; i++)
        begin
            sub_status[i] <= sub_status[i-1];
        end
        sub_regs.done <= sub_status[SUBTRACT_LATENCY-1];
    end
    fp_subtract_arria10
    subtract
    (
        .clk,
        .areset(reset),
        .a(sub_regs.leftoperand),
        .b(sub_regs.rightoperand),
        .q(sub_regs.result)
    );

    localparam MULTIPLY_LATENCY = 2;
    logic [MULTIPLY_LATENCY-1:0] mult_status = 0;
    fp_compute_regs mult_regs;
    always_ff @(posedge clk)
    begin
        mult_status[0] <= mult_regs.trigger;
        for (int i = 1; i < MULTIPLY_LATENCY; i++)
        begin
            mult_status[i] <= mult_status[i-1];
        end
        mult_regs.done <= mult_status[MULTIPLY_LATENCY-1];
    end
    fp_mult_arria10
    multiply
    (
        .clk,
        .areset(reset),
        .a(mult_regs.leftoperand),
        .b(mult_regs.rightoperand),
        .q(mult_regs.result)
    );

    always_ff @(posedge clk)
    begin

        sub_regs.trigger <= 1'b0;
        mult_regs.trigger <= 1'b0;
        FIFO_dot.re <= 1'b0;
        MEM_labels.re <= 1'b0;
        MEM_labels.we <= 1'b0;
        FIFO_gradient.we <= 1'b0;
        op_done <= 1'b0;

        if (reset)
        begin
            modify_state <= STATE_IDLE;
        end
        else
        begin
            case (modify_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        offsetByIndex <= regs[0] >> 4;
                        positionByIndex <= regs[0][3:0];
                        MEM_labels_load_offset <= regs[3][15:0];
                        // MEM_labels_store_offset <= regs[3][31:16];
                        model_type <= regs[4][1:0];
                        algorithm_type <= regs[4][2];
                        step_size <= regs[5];
                        lambda <= regs[6];
                        modify_state <= (regs[4][2] == 1'b0) ? STATE_SGD_MAIN : STATE_SCD_MAIN;
                    end
                end

                STATE_SGD_MAIN:
                begin
                    if (!FIFO_dot.empty)
                    begin
                        FIFO_dot.re <= 1'b1;
                        MEM_labels.re <= 1'b1;
                        MEM_labels.raddr <= MEM_labels_load_offset + offsetByIndex;
                    end

                    if (FIFO_dot.rvalid && MEM_labels.rvalid)
                    begin
                        sub_regs.trigger <= 1'b1;
                        sub_regs.leftoperand <= FIFO_dot.rdata;
                        sub_regs.rightoperand <= MEM_labels.rdata[positionByIndex*32+31 -: 32];
                    end

                    if (sub_regs.done)
                    begin
                        mult_regs.trigger <= 1'b1;
                        mult_regs.leftoperand <= step_size;
                        mult_regs.rightoperand <= sub_regs.result;
                    end

                    if (mult_regs.done)
                    begin
                        op_done <= 1'b1;
                        FIFO_gradient.we <= 1'b1;
                        FIFO_gradient.wdata <= mult_regs.result;
                        modify_state <= STATE_IDLE;
                    end
                end

                STATE_SCD_MAIN:
                begin
                    if (!FIFO_dot.empty)
                    begin
                        FIFO_dot.re <= 1'b1;
                    end

                    if (FIFO_dot.rvalid)
                    begin
                        mult_regs.trigger <= 1'b1;
                        mult_regs.leftoperand <= step_size;
                        mult_regs.rightoperand <= FIFO_dot.rdata;;
                    end

                    if (mult_status[0])
                    begin
                        MEM_labels.re <= 1'b1;
                        MEM_labels.raddr <= MEM_labels_load_offset + offsetByIndex;
                    end

                    if (mult_regs.done)
                    begin
                        sub_regs.trigger <= 1'b1;
                        sub_regs.leftoperand <= MEM_labels.rdata[positionByIndex*32+31 -: 32];
                        sub_regs.rightoperand <= mult_regs.result;
                        lineFromLabelsMem <= MEM_labels.rdata;
                        FIFO_gradient.we <= 1'b1;
                        FIFO_gradient.wdata <= mult_regs.result;
                    end

                    if (sub_regs.done)
                    begin
                        op_done <= 1'b1;
                        MEM_labels.we <= 1'b1;
                        MEM_labels.waddr <= MEM_labels_load_offset + offsetByIndex;
                        MEM_labels.wdata <= lineFromLabelsMem;
                        MEM_labels.wdata[positionByIndex*32+31 -: 32] <= sub_regs.result;
                        modify_state <= STATE_IDLE;
                    end
                end

            endcase
        end
    end

endmodule // glm_modify