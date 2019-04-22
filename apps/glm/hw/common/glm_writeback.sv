`include "pipearch_common.vh"
`include "glm_common.vh"

module glm_writeback
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [6+NUM_WRITEBACK_CHANNELS],
    input t_claddr in_addr,
    input t_claddr out_addr,

    fifobram_interface.read REGION0_read,
    fifobram_interface.read REGION1_read,
    fifobram_interface.read REGION2_read,

    dma_write_interface.to_dma DMA_write
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_PREPROCESS,
        STATE_TRIGGER,
        STATE_WRITE,
        STATE_DONE
    } t_writestate;
    t_writestate send_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic[31:0] offset_by_index[3];
    t_claddr DRAM_store_offset;
    logic [32:0] DRAM_store_length;
    logic [3:0] select_channel;
    logic write_fence;

    internal_interface #(.WIDTH(CLDATA_WIDTH)) to_writeback();
    // *************************************************************************
    //
    //   Store Channels
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(1)) dummy_accessprops_read[2]();

    internal_interface #(.WIDTH(CLDATA_WIDTH)) to_writeback_from_REGION0();
    read_region
    read_REGION0 (
        .clk, .reset,
        .op_start(DMA_write.control.start),
        .configreg(regs[6]),
        .iterations(16'd1),
        .region_access(REGION0_read),
        .props_access(dummy_accessprops_read[0].read),
        .outfrom_read(to_writeback_from_REGION0.commonread_source)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) to_writeback_from_REGION1();
    read_region
    read_REGION1 (
        .clk, .reset,
        .op_start(DMA_write.control.start),
        .configreg(regs[7]),
        .iterations(16'd1),
        .region_access(REGION1_read),
        .props_access(dummy_accessprops_read[1].read),
        .outfrom_read(to_writeback_from_REGION1.commonread_source)
    );

    internal_interface #(.WIDTH(CLDATA_WIDTH)) to_writeback_from_REGION2();
    read_region
    read_REGION2 (
        .clk, .reset,
        .op_start(DMA_write.control.start),
        .configreg(regs[8]),
        .iterations(16'd1),
        .region_access(REGION2_read),
        .props_access(dummy_accessprops_read[1].read),
        .outfrom_read(to_writeback_from_REGION2.commonread_source)
    );

    always_ff @(posedge clk)
    begin
        if (select_channel == 0)
        begin
            to_writeback.rvalid <= to_writeback_from_REGION0.rvalid;
            to_writeback.rdata <= to_writeback_from_REGION0.rdata;
            to_writeback_from_REGION0.almostfull <= to_writeback.almostfull;
        end
        else if (select_channel == 1)
        begin
            to_writeback.rvalid <= to_writeback_from_REGION1.rvalid;
            to_writeback.rdata <= to_writeback_from_REGION1.rdata;
            to_writeback_from_REGION1.almostfull <= to_writeback.almostfull;
        end
        else if (select_channel == 2)
        begin
            to_writeback.rvalid <= to_writeback_from_REGION2.rvalid;
            to_writeback.rdata <= to_writeback_from_REGION2.rdata;
            to_writeback_from_REGION2.almostfull <= to_writeback.almostfull;
        end
        to_writeback.almostfull <= DMA_write.rx_write.walmostfull || !DMA_write.status.active || (send_state != STATE_WRITE);
    end

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [1:0] offset_accumulate;
    logic [31:0] num_sent_lines;
    logic [31:0] num_ack_lines;

    always_ff @(posedge clk)
    begin

        DMA_write.control.start <= 1'b0;
        DMA_write.tx_write.we <= 1'b0;
        op_done <= 1'b0;

        // =================================
        //
        //   Send State Machine
        //
        // =================================
        case(send_state)
            STATE_IDLE:
            begin
                if (op_start)
                begin
                    // *************************************************************************
                    offset_by_index[0] <= regs[0];
                    offset_by_index[1] <= regs[1];
                    offset_by_index[2] <= regs[2];
                    DRAM_store_offset <= (regs[3][31] == 1'b0) ? out_addr + regs[3][30:0] : in_addr + regs[3][30:0];
                    DRAM_store_length <= regs[4];
                    select_channel <= regs[5][3:0];
                    write_fence <= regs[5][4];
                    // *************************************************************************
                    offset_accumulate <= 2'b0;
                    num_sent_lines <= 32'b0;
                    num_ack_lines <= 32'b0;
                    if (regs[4] == 0)
                    begin
                        send_state <= STATE_DONE;
                    end
                    else
                    begin
                        send_state <= STATE_PREPROCESS;
                    end
                end
            end

            STATE_PREPROCESS:
            begin
                DRAM_store_offset <= DRAM_store_offset + offset_by_index[offset_accumulate];
                offset_accumulate <= offset_accumulate + 1;
                if (offset_accumulate == 2)
                begin
                    send_state <= STATE_TRIGGER;
                end
            end

            STATE_TRIGGER:
            begin
                if (DMA_write.status.idle)
                begin
                    DMA_write.control.start <= 1'b1;
                    DMA_write.control.regs <= t_dma_reg'(0);
                    DMA_write.control.regs.reg4 <= DRAM_store_length;
                    DMA_write.control.addr <= DRAM_store_offset;
                    send_state <= STATE_WRITE;
                end
            end

            STATE_WRITE:
            begin
                if (DMA_write.status.active && to_writeback.rvalid)
                begin
                    DMA_write.tx_write.we <= 1'b1;
                    DMA_write.tx_write.wdata <= to_writeback.rdata;

                    num_sent_lines <= num_sent_lines + 1;
                    if (num_sent_lines == DRAM_store_length-1 && write_fence == 1'b0)
                    begin
                        send_state <= STATE_DONE;
                    end
                end

                if (DMA_write.rx_write.wvalid)
                begin
                    num_ack_lines <= num_ack_lines + 1;
                    if (num_ack_lines == DRAM_store_length-1 && write_fence == 1'b1)
                    begin
                        send_state <= STATE_DONE;
                    end
                end

                if (DMA_write.status.done)
                begin
                    send_state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                op_done <= 1'b1;
                send_state <= STATE_IDLE;
            end
        endcase

        if (reset)
        begin
            send_state <= STATE_IDLE;
        end
    end

endmodule // glm_writeback