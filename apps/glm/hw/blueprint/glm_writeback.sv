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

    fifobram_interface.bram_read MEM_model,
    fifobram_interface.bram_read MEM_labels,

    dma_control.to_dma dma_write_control,
    dma_interface.to_dma_write dma_write
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_PREPROCESS,
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

    internal_interface #(.WIDTH(512)) to_writeback();
    // *************************************************************************
    //
    //   Store Channels
    //
    // *************************************************************************
    internal_interface #(.WIDTH(512)) to_writeback_from_MEM_model();
    read_bram
    read_MEM_model_inst (
        .clk, .reset,
        .op_start(dma_write_control.start),
        .configreg(regs[6]),
        .memory_access(MEM_model),
        .outfrom_read(to_writeback_from_MEM_model.commonread_source)
    );

    internal_interface #(.WIDTH(512)) to_writeback_from_MEM_labels();
    read_bram
    read_MEM_labels_inst (
        .clk, .reset,
        .op_start(dma_write_control.start),
        .configreg(regs[7]),
        .memory_access(MEM_labels),
        .outfrom_read(to_writeback_from_MEM_labels.commonread_source)
    );

    always_ff @(posedge clk)
    begin
        if (select_channel == 0)
        begin
            to_writeback.rvalid <= to_writeback_from_MEM_model.rvalid;
            to_writeback.rdata <= to_writeback_from_MEM_model.rdata;
            to_writeback_from_MEM_model.almostfull <= to_writeback.almostfull;
        end
        else if (select_channel == 1)
        begin
            to_writeback.rvalid <= to_writeback_from_MEM_labels.rvalid;
            to_writeback.rdata <= to_writeback_from_MEM_labels.rdata;
            to_writeback_from_MEM_labels.almostfull <= to_writeback.almostfull;
        end
        to_writeback.almostfull <= dma_write.tx_walmostfull || (send_state == STATE_PREPROCESS);
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

        dma_write_control.start <= 1'b0;
        dma_write.tx_we <= 1'b0;
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
                    send_state <= STATE_WRITE;
                end
            end

            STATE_WRITE:
            begin
                if (!dma_write_control.active)
                begin
                    dma_write_control.start <= 1'b1;
                    dma_write_control.regs[0] <= 0;
                    dma_write_control.regs[1] <= 0;
                    dma_write_control.regs[2] <= 0;
                    dma_write_control.regs[3] <= 0;
                    dma_write_control.regs[4] <= DRAM_store_length;
                    dma_write_control.addr <= DRAM_store_offset;
                end
                else if (to_writeback.rvalid)
                begin
                    dma_write.tx_we <= 1'b1;
                    dma_write.tx_wdata <= to_writeback.rdata;

                    num_sent_lines <= num_sent_lines + 1;
                    if (num_sent_lines == DRAM_store_length-1 && write_fence == 1'b0)
                    begin
                        send_state <= STATE_DONE;
                    end
                end

                if (dma_write.rx_wvalid)
                begin
                    num_ack_lines <= num_ack_lines + 1;
                    if (num_ack_lines == DRAM_store_length-1 && write_fence == 1'b1)
                    begin
                        send_state <= STATE_DONE;
                    end
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