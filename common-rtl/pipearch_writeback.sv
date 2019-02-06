`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module pipearch_writeback
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [5],
    input t_ccip_clAddr in_addr,
    input t_ccip_clAddr out_addr,

    internal_interface.from_commonread outfrom_read,

    // CCI-P request/response
    input  logic c1TxAlmFull,
    input  t_if_ccip_c1_Rx cp2af_sRx_c1,
    output t_if_ccip_c1_Tx af2cp_sTx_c1
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_WRITE,
        STATE_DONE
    } t_writestate;
    t_writestate send_state;
    t_writestate ack_state;

    t_ccip_clAddr DRAM_store_offset;
    logic [15:0] DRAM_store_length;

    t_cci_c1_ReqMemHdr wr_hdr;
    always_comb
    begin
        wr_hdr = t_cci_c1_ReqMemHdr'(0);
        // Write request type
        wr_hdr.req_type = eREQ_WRLINE_I;
        // Let the FIU pick the channel
        wr_hdr.vc_sel = eVC_VA;
        // Write 1 line
        wr_hdr.cl_len = eCL_LEN_1;
        // Start of packet is true (single line write)
        wr_hdr.sop = 1'b1;
    end

    // Counters
    logic [15:0] num_sent_lines;
    logic [15:0] num_ack_lines;

    assign outfrom_read.almostfull = c1TxAlmFull;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            send_state <= STATE_IDLE;
            ack_state <= STATE_IDLE;
            num_sent_lines <= 16'b0;
            num_ack_lines <= 16'b0;
            af2cp_sTx_c1.valid <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin
            // =================================
            //
            //   Send State Machine
            //
            // =================================
            af2cp_sTx_c1.valid <= 1'b0;
            case(send_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        DRAM_store_offset <= (regs[3][31] == 1'b0) ? out_addr + regs[3] : in_addr + regs[3];
                        DRAM_store_length <= regs[4];
                        num_sent_lines <= 16'b0;
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
                    if (outfrom_read.rvalid)
                    begin
                        af2cp_sTx_c1.valid <= 1'b1;
                        af2cp_sTx_c1.data <= outfrom_read.rdata;
                        af2cp_sTx_c1.hdr <= wr_hdr;
                        af2cp_sTx_c1.hdr.address <= DRAM_store_offset + num_sent_lines;
                        num_sent_lines <= num_sent_lines + 1;
                        if (num_sent_lines == DRAM_store_length-1)
                        begin
                            send_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    send_state <= STATE_IDLE;
                end
            endcase

            // =================================
            //
            //   Ack State Machine
            //
            // =================================
            op_done <= 1'b0;
            case(ack_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        num_ack_lines <= 16'b0;
                        if (regs[4] == 0)
                        begin
                            ack_state <= STATE_DONE;
                        end
                        else
                        begin
                            ack_state <= STATE_WRITE;
                        end
                    end
                end

                STATE_WRITE:
                begin
                    if (cp2af_sRx_c1.rspValid)
                    begin
                        num_ack_lines <= num_ack_lines + 1;
                        if (num_ack_lines == DRAM_store_length-1)
                        begin
                            ack_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    ack_state <= STATE_IDLE;
                end
            endcase

        end
    end


endmodule // pipearch_writeback
