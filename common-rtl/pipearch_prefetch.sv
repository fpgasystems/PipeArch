`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module pipearch_prefetch
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],
    input t_ccip_clAddr in_addr,

    // CCI-P request/response
    input  c0TxAlmFull,
    input  t_if_ccip_c0_Rx cp2af_sRx_c0,
    output t_if_ccip_c0_Tx af2cp_sTx_c0,

    output logic get_c0TxAlmFull,
    output t_if_ccip_c0_Rx get_cp2af_sRx_c0,
    input  t_if_ccip_c0_Tx get_af2cp_sTx_c0
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;
    t_readstate receive_state;

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) prefetch_fifo_access();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .reset,
        .access(prefetch_fifo_access.fifo_source)
    );
    assign prefetch_fifo_access.we = cci_c0Rx_isReadRsp(cp2af_sRx_c0) && (receive_state == STATE_READ);
    assign prefetch_fifo_access.wdata = cp2af_sRx_c0.data;

    fifobram_interface #(.WIDTH(LOG2_PREFETCH_SIZE), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) wait_fifo_access();
    fifo
    #(.WIDTH(LOG2_PREFETCH_SIZE), .LOG2_DEPTH(LOG2_PREFETCH_SIZE))
    wait_fifo
    (
        .clk,
        .reset,
        .access(wait_fifo_access.fifo_source)
    );
    always_ff @(posedge clk)
    begin
        wait_fifo_access.re <= (!wait_fifo_access.empty) && !(prefetch_fifo_access.empty);
        prefetch_fifo_access.re <= (!wait_fifo_access.empty) && !(prefetch_fifo_access.empty);
    end

    t_cci_c0_ReqMemHdr rd_hdr;
    always_comb
    begin
        rd_hdr = t_cci_c0_ReqMemHdr'(0);
        rd_hdr.req_type = eREQ_RDLINE_I;
        rd_hdr.vc_sel = eVC_VA;
        rd_hdr.cl_len = eCL_LEN_1;
    end

    t_cci_c0_RspMemHdr response_hdr;
    always_comb
    begin
        response_hdr = t_cci_c0_RspMemHdr'(0);
        response_hdr.vc_used = eVC_VA;
        response_hdr.hit_miss = 1'b0;
        response_hdr.cl_num = 2'b0;
        response_hdr.resp_type = eRSP_RDLINE;
        response_hdr.mdata = 0;
    end

    // Local variables
    t_ccip_clAddr DRAM_load_offset;
    logic [31:0] DRAM_load_length;

    // Counters
    logic [31:0] num_wait_fifo_lines;
    logic [31:0] num_requested_lines;
    logic [31:0] num_received_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    always_ff @(posedge clk)
    begin
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_access.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);

        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;

            wait_fifo_access.we <= 1'b0;

            num_wait_fifo_lines <= 32'b0;
            num_requested_lines <= 32'b0;
            num_received_lines <= 32'b0;

            af2cp_sTx_c0.valid <= 1'b0;
            get_cp2af_sRx_c0.rspValid <= 1'b0;

            op_done <= 1'b0;
        end
        else
        begin
            // =================================
            //
            //   Request State Machine
            //
            // =================================
            wait_fifo_access.we <= 1'b0;
            af2cp_sTx_c0.valid <= 1'b0;
            case (request_state)
                STATE_IDLE:
                begin
                    get_c0TxAlmFull <= c0TxAlmFull;
                    af2cp_sTx_c0 <= get_af2cp_sTx_c0;

                    if (op_start)
                    begin
                        DRAM_load_offset <= in_addr + regs[3];
                        DRAM_load_length <= regs[4];
                        num_wait_fifo_lines <= 32'b0;
                        num_requested_lines <= 32'b0;
                        request_state <= STATE_READ;
                    end
                end

                STATE_READ:
                begin
                    if (num_requested_lines < DRAM_load_length && !c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                    begin
                        af2cp_sTx_c0.valid <= 1'b1;
                        af2cp_sTx_c0.hdr <= rd_hdr;
                        af2cp_sTx_c0.hdr.address <= t_ccip_clAddr'(DRAM_load_offset + num_requested_lines);
                        af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];

                        num_requested_lines <= num_requested_lines + 1;
                    end

                    get_c0TxAlmFull <= c0TxAlmFull || wait_fifo_access.almostfull;
                    if (get_af2cp_sTx_c0.valid)
                    begin
                        wait_fifo_access.we <= 1'b1;
                        wait_fifo_access.wdata <= num_wait_fifo_lines[LOG2_PREFETCH_SIZE-1:0];
                        num_wait_fifo_lines <= num_wait_fifo_lines + 1;
                    end

                    if (num_requested_lines == DRAM_load_length && num_wait_fifo_lines == DRAM_load_length)
                    begin
                        request_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    request_state <= STATE_IDLE;
                end
            endcase

            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            get_cp2af_sRx_c0.rspValid <= 1'b0;
            op_done <= 1'b0;
            case (receive_state)
                STATE_IDLE:
                begin
                    get_cp2af_sRx_c0 <= cp2af_sRx_c0;

                    if (op_start)
                    begin
                        num_received_lines <= 32'b0;
                        receive_state <= STATE_READ;
                    end
                end

                STATE_READ:
                begin
                    if (prefetch_fifo_access.rvalid)
                    begin
                        get_cp2af_sRx_c0.rspValid <= 1'b1;
                        get_cp2af_sRx_c0.hdr <= response_hdr;
                        get_cp2af_sRx_c0.data <= prefetch_fifo_access.rdata;
                        num_received_lines <= num_received_lines + 1;
                    end

                    if (num_received_lines == DRAM_load_length && wait_fifo_access.count[LOG2_PREFETCH_SIZE-1:0] == LOG2_PREFETCH_SIZE'(0))
                    begin
                        receive_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    receive_state <= STATE_IDLE;
                end
            endcase
        end
    end


endmodule // pipearch_prefetch