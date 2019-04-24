`include "cci_mpf_if.vh"
`include "pipearch_common.vh"

module pipearch_dma_read
(
    input  logic clk,
    input  logic reset,

    // CCI-P request/response
    input  c0TxAlmFull,
    input  t_if_ccip_c0_Rx cp2af_sRx_c0,
    output t_if_ccip_c0_Tx af2cp_sTx_c0,
    input t_ccip_vc vc_select,

    dma_read_interface.at_dma DMA_read
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
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;
    t_readstate receive_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic[31:0] offset_by_index[3];
    t_claddr DRAM_load_offset;
    t_claddr running_DRAM_load_offset;
    logic [31:0] DRAM_load_length;
    logic [31:0] DRAM_receive_length;
    logic enable_multiline;

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [1:0] offset_accumulate;
    logic [31:0] num_wait_fifo_lines;
    logic [31:0] num_requested_lines;
    logic [31:0] num_requested_lines_plus2;
    logic [31:0] num_requested_lines_plus4;
    logic [31:0] num_received_lines;
    logic [31:0] num_forward_request_lines;
    logic [31:0] num_forwarded_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    // *************************************************************************
    //
    //  Transaction FIFO
    //
    // *************************************************************************
    logic request_async;
    t_dma_control temp_control;

    parameter TX_FIFO_WIDTH = $bits(t_dma_control);
    fifobram_interface #(.WIDTH(TX_FIFO_WIDTH), .LOG2_DEPTH(6)) tx_fifo_access();
    fifo
    #(.WIDTH(TX_FIFO_WIDTH), .LOG2_DEPTH(6))
    tx_fifo
    (
        .clk,
        .reset,
        .access(tx_fifo_access.fifo_source)
    );
    logic started_request;
    always_ff @(posedge clk)
    begin
        tx_fifo_access.we <= DMA_read.control.start;
        tx_fifo_access.wdata <= DMA_read.control;
        tx_fifo_access.re <= 1'b0;
        if (!tx_fifo_access.empty && started_request == 1'b0)
        begin
            tx_fifo_access.re <= 1'b1;
            started_request <= 1'b1;
        end
        if (reset || request_state == STATE_DONE)
        begin
            started_request <= 1'b0;
        end
    end

    assign temp_control = tx_fifo_access.rdata;

    // *************************************************************************
    //
    //   Receive FIFO
    //
    // *************************************************************************
    fifobram_interface #(.WIDTH($bits(t_cci_c0_RspMemHdr) + 512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) prefetch_fifo_access();
    fifo
    #(.WIDTH($bits(t_cci_c0_RspMemHdr) + 512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .reset,
        .access(prefetch_fifo_access.fifo_source)
    );
    always_ff @(posedge clk)
    begin
        prefetch_fifo_access.wdata <= {cp2af_sRx_c0.hdr, cp2af_sRx_c0.data};
        prefetch_fifo_access.we <= 1'b0;
        if (tx_fifo_access.rvalid)
        begin
            num_received_lines <= 32'b0;
        end
        if (cp2af_sRx_c0.rspValid && receive_state == STATE_READ)
        begin
            prefetch_fifo_access.we <= 1'b1;
            num_received_lines <= num_received_lines + 1;
        end
    end

    assign DMA_read.status.idle = (request_state == STATE_IDLE && receive_state == STATE_IDLE);
    assign DMA_read.status.active = (request_state == STATE_READ);

    always_ff @(posedge clk)
    begin
        // =================================
        //
        //   Calculate Allowed Lines
        //
        // =================================
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_access.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);

        af2cp_sTx_c0.hdr <= t_cci_c0_ReqMemHdr'(0);
        af2cp_sTx_c0.hdr.vc_sel <= vc_select;
        af2cp_sTx_c0.valid <= 1'b0;
        prefetch_fifo_access.re <= 1'b0;
        DMA_read.rx_read.rvalid <= 1'b0;

        // =================================
        //
        //   Request State Machine
        //
        // =================================
        case (request_state)
            STATE_IDLE:
            begin
                DMA_read.rx_read.ralmostfull <= c0TxAlmFull;
                af2cp_sTx_c0.valid <= DMA_read.tx_read.re;
                af2cp_sTx_c0.hdr.address <= DMA_read.tx_read.raddr;
                af2cp_sTx_c0.hdr.cl_len <= t_ccip_clLen'(DMA_read.tx_read.rlength);

                if (tx_fifo_access.rvalid)
                begin
                    // *************************************************************************
                    request_async <= temp_control.async;
                    offset_by_index[0] <= temp_control.regs.reg0;
                    offset_by_index[1] <= temp_control.regs.reg1;
                    offset_by_index[2] <= temp_control.regs.reg2;
                    DRAM_load_offset <= temp_control.addr + temp_control.regs.reg3;
                    running_DRAM_load_offset <= temp_control.addr + temp_control.regs.reg3;
                    DRAM_load_length <= temp_control.regs.reg4[30:0];
                    enable_multiline <= temp_control.regs.reg4[31];
                    num_wait_fifo_lines <= temp_control.async ? temp_control.regs.reg4[30:0] : 32'b0;
                    // *************************************************************************
                    offset_accumulate <= 2'b0;
                    num_requested_lines <= 32'b0;
                    num_requested_lines_plus4 <= 32'd4;
                    num_requested_lines_plus2 <= 32'd2;
                    request_state <= STATE_PREPROCESS;
                end
            end

            STATE_PREPROCESS:
            begin
                DRAM_load_offset <= DRAM_load_offset + offset_by_index[offset_accumulate];
                running_DRAM_load_offset <= running_DRAM_load_offset + offset_by_index[offset_accumulate];
                offset_accumulate <= offset_accumulate + 1;
                if (offset_accumulate == 2)
                begin
                    request_state <= STATE_READ;
                end
            end

            STATE_READ:
            begin
                if (!c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                begin

                    if (enable_multiline && 
                        (num_requested_lines_plus4 < DRAM_load_length) && 
                        (running_DRAM_load_offset[1:0] == 2'b0))
                    begin
                        af2cp_sTx_c0.valid <= 1'b1;
                        af2cp_sTx_c0.hdr.address <= t_claddr'(running_DRAM_load_offset);
                        af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                        af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_4;
                        num_requested_lines <= num_requested_lines_plus4;
                        num_requested_lines_plus2 <= num_requested_lines_plus4 + 2;
                        num_requested_lines_plus4 <= num_requested_lines_plus4 + 4;
                        running_DRAM_load_offset <= running_DRAM_load_offset + 4;
                    end
                    else if (enable_multiline && 
                        (num_requested_lines_plus2 < DRAM_load_length) &&
                        (running_DRAM_load_offset[0] == 1'b0))
                    begin
                        af2cp_sTx_c0.valid <= 1'b1;
                        af2cp_sTx_c0.hdr.address <= t_claddr'(running_DRAM_load_offset);
                        af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                        af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_2;
                        num_requested_lines <= num_requested_lines_plus2;
                        num_requested_lines_plus2 <= num_requested_lines_plus2 + 2;
                        num_requested_lines_plus4 <= num_requested_lines_plus2 + 4;
                        running_DRAM_load_offset <= running_DRAM_load_offset + 2;
                    end
                    else if (num_requested_lines < DRAM_load_length)
                    begin
                        af2cp_sTx_c0.valid <= 1'b1;
                        af2cp_sTx_c0.hdr.address <= t_claddr'(running_DRAM_load_offset);
                        af2cp_sTx_c0.hdr.mdata <= num_requested_lines[15:0];
                        af2cp_sTx_c0.hdr.cl_len <= eCL_LEN_1;
                        num_requested_lines <= num_requested_lines + 1;
                        num_requested_lines_plus2 <= num_requested_lines + 3;
                        num_requested_lines_plus4 <= num_requested_lines + 5;
                        running_DRAM_load_offset <= running_DRAM_load_offset + 1;
                    end

                end

                DMA_read.rx_read.ralmostfull <= 1'b0;
                if (DMA_read.tx_read.re && !request_async)
                begin
                    if (DMA_read.tx_read.rlength == 2'b11) begin
                        num_wait_fifo_lines <= num_wait_fifo_lines + 4;
                    end
                    else if (DMA_read.tx_read.rlength == 2'b01) begin
                        num_wait_fifo_lines <= num_wait_fifo_lines + 2;
                    end
                    else if (DMA_read.tx_read.rlength == 2'b00) begin
                        num_wait_fifo_lines <= num_wait_fifo_lines + 1;
                    end
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
        DMA_read.rx_read.rvalid <= 1'b0;
        DMA_read.status.done <= 1'b0;
        case (receive_state)
            STATE_IDLE:
            begin
                DMA_read.rx_read.rvalid <= cp2af_sRx_c0.rspValid;
                DMA_read.rx_read.rdata <= cp2af_sRx_c0.data;
                if (tx_fifo_access.rvalid && !temp_control.async)
                begin
                    // *************************************************************************
                    DRAM_receive_length <= temp_control.regs.reg4[30:0];
                    // *************************************************************************
                    num_forward_request_lines <= 32'b0;
                    num_forwarded_lines <= 32'b0;
                    receive_state <= STATE_READ;
                end
            end

            STATE_READ:
            begin
                if (!prefetch_fifo_access.empty && num_forward_request_lines < num_wait_fifo_lines)
                begin
                    prefetch_fifo_access.re <= 1'b1;
                    num_forward_request_lines <= num_forward_request_lines + 1;
                end

                if (prefetch_fifo_access.rvalid)
                begin
                    DMA_read.rx_read.rvalid <= 1'b1;
                    DMA_read.rx_read.rdata <= prefetch_fifo_access.rdata[511:0];
                    num_forwarded_lines <= num_forwarded_lines + 1;
                end

                if (num_forwarded_lines == DRAM_receive_length)
                begin
                    receive_state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                DMA_read.status.done <= 1'b1;
                receive_state <= STATE_IDLE;
            end
        endcase

        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_dma_read