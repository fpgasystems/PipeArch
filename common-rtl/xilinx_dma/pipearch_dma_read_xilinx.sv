`include "pipearch_common.vh"

module pipearch_dma_read_xilinx
# (
    parameter integer  C_M_AXI_GMEM_ID_WIDTH = 1,
    parameter integer  C_M_AXI_GMEM_ADDR_WIDTH = 42,
    parameter integer  C_M_AXI_GMEM_DATA_WIDTH = 512
)
(
    input  logic clk,
    input  logic reset,

    // AXI Master
    output wire                                 m_axi_gmem_ARVALID,
    input  wire                                 m_axi_gmem_ARREADY,
    output wire [C_M_AXI_GMEM_ADDR_WIDTH-1:0]   m_axi_gmem_ARADDR,
    output wire [C_M_AXI_GMEM_ID_WIDTH-1:0]     m_axi_gmem_ARID,
    output wire [7:0]                           m_axi_gmem_ARLEN,
    output wire [2:0]                           m_axi_gmem_ARSIZE,
    output wire [1:0]                           m_axi_gmem_ARBURST,
    output wire [1:0]                           m_axi_gmem_ARLOCK,
    output wire [3:0]                           m_axi_gmem_ARCACHE,
    output wire [2:0]                           m_axi_gmem_ARPROT,
    output wire [3:0]                           m_axi_gmem_ARQOS,
    output wire [3:0]                           m_axi_gmem_ARREGION,
    input  wire                                 m_axi_gmem_RVALID,
    output wire                                 m_axi_gmem_RREADY,
    input  wire [C_M_AXI_GMEM_DATA_WIDTH - 1:0] m_axi_gmem_RDATA,
    input  wire                                 m_axi_gmem_RLAST,
    input  wire [C_M_AXI_GMEM_ID_WIDTH - 1:0]   m_axi_gmem_RID,
    input  wire [1:0]                           m_axi_gmem_RRESP,

    dma_read_interface.at_dma DMA_read
);

    ///////////////////////////////////////////////////////////////////////////////
    // RTL Logic 
    ///////////////////////////////////////////////////////////////////////////////
    // Tie-off unused AXI protocol features
    assign m_axi_gmem_ARBURST  = 2'b01;
    assign m_axi_gmem_ARLOCK   = 2'b00;
    assign m_axi_gmem_ARCACHE  = 4'b0011;
    assign m_axi_gmem_ARPROT   = 3'b000;
    assign m_axi_gmem_ARQOS    = 4'b0000;
    assign m_axi_gmem_ARREGION = 4'b0000;

    localparam integer LP_DW_BYTES = CLDATA_WIDTH/8;
    localparam integer LP_AXI_BURST_LEN = 4096/LP_DW_BYTES < 256 ? 4096/LP_DW_BYTES : 256;
    localparam integer LP_LOG_BURST_LEN = $clog2(LP_AXI_BURST_LEN);
    localparam integer LP_RD_MAX_OUTSTANDING = 3;
    localparam integer LP_RM_MAX_OUTSTANDING_LINES = LP_RD_MAX_OUTSTANDING*LP_AXI_BURST_LEN;

    // AXI4 Read Master
    logic krnl_ctrl_start;
    logic krnl_ctrl_done;
    logic krnl_ctrl_done_issued;
    logic krnl_ctrl_prog_full;
    logic [CLADDR_WIDTH-1:0] krnl_ctrl_addr;
    logic [31:0] krnl_ctrl_length;
    krnl_axi_read_master #( 
        .C_ADDR_WIDTH       (CLADDR_WIDTH) ,
        .C_DATA_WIDTH       (CLDATA_WIDTH) ,
        .C_ID_WIDTH         (1) ,
        .C_NUM_CHANNELS     (1) ,
        .C_LENGTH_WIDTH     (32) ,
        .C_BURST_LEN        (LP_AXI_BURST_LEN) ,
        .C_LOG_BURST_LEN    (LP_LOG_BURST_LEN) ,
        .C_MAX_OUTSTANDING  (LP_RD_MAX_OUTSTANDING)
    )
    inst_axi_read_master ( 
        .aclk           (clk),
        .areset         (reset),

        .ctrl_start     (krnl_ctrl_start),
        .ctrl_done      (krnl_ctrl_done),
        .ctrl_offset    (krnl_ctrl_addr) ,
        .ctrl_length    (krnl_ctrl_length) ,
        .ctrl_prog_full (krnl_ctrl_prog_full) ,

        .arvalid        (m_axi_gmem_ARVALID),
        .arready        (m_axi_gmem_ARREADY),
        .araddr         (m_axi_gmem_ARADDR),
        .arid           (m_axi_gmem_ARID),
        .arlen          (m_axi_gmem_ARLEN),
        .arsize         (m_axi_gmem_ARSIZE),
        .rvalid         (m_axi_gmem_RVALID),
        .rready         (m_axi_gmem_RREADY),
        .rdata          (m_axi_gmem_RDATA),
        .rlast          (m_axi_gmem_RLAST),
        .rid            (m_axi_gmem_RID),
        .rresp          (m_axi_gmem_RRESP),

        .m_tvalid       (prefetch_fifo_access.we),
        .m_tready       (!prefetch_fifo_access.almostfull),
        .m_tdata        (prefetch_fifo_access.wdata)
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
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;

    // *************************************************************************
    //
    //   Instruction Information
    //
    // *************************************************************************
    logic[31:0] offset_by_index[3];
    t_claddr DRAM_load_offset;
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
    logic [31:0] num_received_lines;
    logic [31:0] num_forward_request_lines;
    logic [31:0] num_forwarded_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;

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
    fifobram_interface #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) prefetch_fifo_access();
    fifo
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .reset,
        .access(prefetch_fifo_access.fifo_source)
    );
    always_ff @(posedge clk)
    begin
        if (tx_fifo_access.rvalid)
        begin
            num_received_lines <= 32'b0;
        end
        if (prefetch_fifo_access.we)
        begin
            num_received_lines <= num_received_lines + 1;
        end
    end

    assign DMA_read.status.idle = (request_state == STATE_IDLE);
    assign DMA_read.status.active = (request_state == STATE_READ);

    always_ff @(posedge clk)
    begin
        // =================================
        //
        //   Calculate Allowed Lines
        //
        // =================================
        num_lines_in_flight <= num_requested_lines - num_forwarded_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - num_lines_in_flight;
        krnl_ctrl_prog_full <= prefetch_fifo_free_count < LP_RM_MAX_OUTSTANDING_LINES;

        krnl_ctrl_start <= 1'b0;
        prefetch_fifo_access.re <= 1'b0;
        DMA_read.rx_read.rvalid <= 1'b0;
        DMA_read.status.done <= 1'b0;

        // =================================
        //
        //   Request State Machine
        //
        // =================================
        case (request_state)
            STATE_IDLE:
            begin
                if (tx_fifo_access.rvalid)
                begin
                    // *************************************************************************
                    request_async <= temp_control.async;
                    offset_by_index[0] <= temp_control.regs.reg0;
                    offset_by_index[1] <= temp_control.regs.reg1;
                    offset_by_index[2] <= temp_control.regs.reg2;
                    DRAM_load_offset <= temp_control.addr + temp_control.regs.reg3;
                    DRAM_load_length <= temp_control.regs.reg4[30:0];
                    enable_multiline <= temp_control.regs.reg4[31];
                    num_wait_fifo_lines <= temp_control.async ? temp_control.regs.reg4[30:0] : 32'b0;
                    // *************************************************************************
                    offset_accumulate <= 2'b0;
                    num_requested_lines <= 0;
                    num_forward_request_lines <= 0;
                    num_forwarded_lines <= 0;
                    request_state <= STATE_PREPROCESS;
                end
            end

            STATE_PREPROCESS:
            begin
                DRAM_load_offset <= DRAM_load_offset + offset_by_index[offset_accumulate];
                offset_accumulate <= offset_accumulate + 1;
                if (offset_accumulate == 2)
                begin
                    request_state <= STATE_TRIGGER;
                end
            end

            STATE_TRIGGER:
            begin
                krnl_ctrl_start <= 1'b1;
                krnl_ctrl_addr <= (DRAM_load_offset << CL_BYTE_IDX_BITS);
                krnl_ctrl_length <= DRAM_load_length;
                krnl_ctrl_done_issued <= 1'b0;
                request_state <= STATE_READ;
            end

            STATE_READ:
            begin
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

                if (m_axi_gmem_ARVALID)
                begin
                    num_requested_lines <= num_requested_lines + m_axi_gmem_ARLEN + 1;
                end

                if (!prefetch_fifo_access.empty && num_forward_request_lines < num_wait_fifo_lines)
                begin
                    prefetch_fifo_access.re <= 1'b1;
                    num_forward_request_lines <= num_forward_request_lines + 1;
                end

                if (prefetch_fifo_access.rvalid)
                begin
                    DMA_read.rx_read.rvalid <= 1'b1;
                    DMA_read.rx_read.rdata <= prefetch_fifo_access.rdata;
                    num_forwarded_lines <= num_forwarded_lines + 1;
                end

                if (krnl_ctrl_done)
                begin
                    krnl_ctrl_done_issued <= 1'b1;
                end

                if (num_forwarded_lines == DRAM_load_length && krnl_ctrl_done_issued)
                begin
                    request_state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                DMA_read.status.done <= 1'b1;
                request_state <= STATE_IDLE;
            end
        endcase

        if (reset)
        begin
            request_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_dma_read_xilinx