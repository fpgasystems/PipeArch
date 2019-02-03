`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module glm_writeback
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],
    input t_ccip_clAddr in_addr,
    input t_ccip_clAddr out_addr,

    fifobram_interface.bram_read MEM_model,
    fifobram_interface.bram_read MEM_labels,

    // CCI-P request/response
    input  logic c1TxAlmFull,
    input  t_if_ccip_c1_Rx cp2af_sRx_c1,
    output t_if_ccip_c1_Tx af2cp_sTx_c1
);

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
        .op_start(op_start),
        .configreg(regs[6]),
        .memory_access(MEM_model),
        .outfrom_read(to_writeback_from_MEM_model.commonread_source)
    );

    internal_interface #(.WIDTH(512)) to_writeback_from_MEM_labels();
    read_bram
    read_MEM_labels_inst (
        .clk, .reset,
        .op_start(op_start),
        .configreg(regs[7]),
        .memory_access(MEM_labels),
        .outfrom_read(to_writeback_from_MEM_labels.commonread_source)
    );

    

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_WRITE,
        STATE_DONE
    } t_writestate;
    t_writestate send_state;

    t_ccip_clAddr DRAM_store_offset;
    logic [15:0] DRAM_store_length;
    logic [3:0] select_channel;
    logic write_fence;

    // Counters
    logic [15:0] num_sent_lines;
    logic [15:0] num_ack_lines;


    always_comb
    begin
        if (select_channel == 0)
        begin
            to_writeback.rvalid = to_writeback_from_MEM_model.rvalid;
            to_writeback.rdata = to_writeback_from_MEM_model.rdata;
            to_writeback_from_MEM_model.almostfull = to_writeback.almostfull;
        end
        else if (select_channel == 1)
        begin
            to_writeback.rvalid = to_writeback_from_MEM_labels.rvalid;
            to_writeback.rdata = to_writeback_from_MEM_labels.rdata;
            to_writeback_from_MEM_labels.almostfull = to_writeback.almostfull;
        end
    end

    assign to_writeback.almostfull = c1TxAlmFull;

    always_ff @(posedge clk)
    begin

        af2cp_sTx_c1.hdr <= t_cci_c1_ReqMemHdr'(0);
        af2cp_sTx_c1.hdr.sop <= 1'b1;
        af2cp_sTx_c1.valid <= 1'b0;
        op_done <= 1'b0;

        if (reset)
        begin
            send_state <= STATE_IDLE;
            num_sent_lines <= 16'b0;
            num_ack_lines <= 16'b0;
            op_done <= 1'b0;
        end
        else
        begin
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
                        DRAM_store_offset <= (regs[3][31] == 1'b0) ? out_addr + regs[3][30:0] : in_addr + regs[3][30:0];
                        DRAM_store_length <= regs[4];
                        select_channel <= regs[5][3:0];
                        write_fence <= regs[5][4];
                        num_sent_lines <= 16'b0;
                        num_ack_lines <= 16'b0;
                        if (regs[4] == 0)
                        begin
                            send_state <= STATE_DONE;
                        end
                        else
                        begin
                            send_state <= STATE_WRITE;
                        end
                    end
                end

                STATE_WRITE:
                begin
                    if (to_writeback.rvalid)
                    begin
                        af2cp_sTx_c1.valid <= 1'b1;
                        af2cp_sTx_c1.data <= to_writeback.rdata;
                        af2cp_sTx_c1.hdr.address <= DRAM_store_offset + num_sent_lines;
                        num_sent_lines <= num_sent_lines + 1;
                        if (num_sent_lines == DRAM_store_length-1 && write_fence == 1'b0)
                        begin
                            send_state <= STATE_DONE;
                        end
                    end

                    if (cp2af_sRx_c1.rspValid)
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
        end
    end


endmodule // glm_writeback