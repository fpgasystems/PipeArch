`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module pipearch_dma_write
(
    input  logic clk,
    input  logic reset,

    // CCI-P request/response
    input  logic c1TxAlmFull,
    input  t_if_ccip_c1_Rx cp2af_sRx_c1,
    output t_if_ccip_c1_Tx af2cp_sTx_c1,
    input t_ccip_vc vc_select,

    dma_control.at_dma get_control,
    dma_interface.from_dma_write get_requests
);

    // *************************************************************************
    //
    //   Internal State
    //
    // *************************************************************************
    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_WRITE,
        STATE_DONE
    } t_writestate;
    t_writestate send_state;
    t_writestate ack_state;


    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    t_claddr DRAM_store_offset;
    logic [31:0] DRAM_store_length;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [31:0] num_sent_lines;
    logic [31:0] num_ack_lines;

    t_cci_c1_ReqMemHdr wr_hdr;
    always_comb
    begin
        wr_hdr = t_cci_c1_ReqMemHdr'(0);
        wr_hdr.req_type = eREQ_WRLINE_I;
        wr_hdr.vc_sel = eVC_VA;
        wr_hdr.cl_len = eCL_LEN_1;
        wr_hdr.sop = 1'b1;
    end

    assign get_control.active = (get_control.start || send_state != STATE_IDLE || ack_state != STATE_IDLE) ? 1'b1 : 1'b0;
    assign get_requests.tx_walmostfull = c1TxAlmFull;

    always_ff @(posedge clk)
    begin
        af2cp_sTx_c1.valid <= 1'b0;
        get_control.done <= 1'b0;
        get_requests.rx_wvalid <= 1'b0;

        af2cp_sTx_c1.hdr <= wr_hdr;
        af2cp_sTx_c1.hdr.vc_sel <= vc_select;

        case(send_state)
            STATE_IDLE:
            begin
                if (get_control.start)
                begin
                    // *************************************************************************
                    DRAM_store_offset <= get_control.addr;
                    DRAM_store_length <= get_control.regs[4];
                    // *************************************************************************
                    num_sent_lines <= 0;
                    num_ack_lines <= 0;
                    send_state <= STATE_WRITE;
                end
            end

            STATE_WRITE:
            begin
                if (get_requests.tx_we)
                begin
                    af2cp_sTx_c1.valid <= 1'b1;
                    af2cp_sTx_c1.data <= get_requests.tx_wdata;
                    af2cp_sTx_c1.hdr.address <= DRAM_store_length + num_sent_lines;
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

        case(ack_state)
            STATE_IDLE:
            begin
                if (get_control.start)
                begin
                    num_ack_lines <= 16'b0;
                    if (get_control.regs[4] == 0)
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
                    get_requests.rx_wvalid <= 1'b1;
                    num_ack_lines <= num_ack_lines + 1;
                    if (num_ack_lines == DRAM_store_length-1)
                    begin
                        ack_state <= STATE_DONE;
                    end
                end
            end

            STATE_DONE:
            begin
                get_control.done <= 1'b1;
                ack_state <= STATE_IDLE;
            end
        endcase

        if (reset)
        begin
            send_state <= STATE_IDLE;
            ack_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_dma_write