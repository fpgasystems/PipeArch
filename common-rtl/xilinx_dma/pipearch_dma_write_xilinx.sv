`include "pipearch_common.vh"

module pipearch_dma_write_xilinx
# (
    parameter integer  C_M_AXI_GMEM_ID_WIDTH = 1,
    parameter integer  C_M_AXI_GMEM_ADDR_WIDTH = 42,
    parameter integer  C_M_AXI_GMEM_DATA_WIDTH = 512
)
(
    input  logic clk,
    input  logic reset,

    // AXI4 master interface 
    output wire                                 m_axi_gmem_AWVALID,
    input  wire                                 m_axi_gmem_AWREADY,
    output wire [C_M_AXI_GMEM_ADDR_WIDTH-1:0]   m_axi_gmem_AWADDR,
    output wire [C_M_AXI_GMEM_ID_WIDTH - 1:0]   m_axi_gmem_AWID,
    output wire [7:0]                           m_axi_gmem_AWLEN,
    output wire [2:0]                           m_axi_gmem_AWSIZE,
    output wire [1:0]                           m_axi_gmem_AWBURST,
    output wire [1:0]                           m_axi_gmem_AWLOCK,
    output wire [3:0]                           m_axi_gmem_AWCACHE,
    output wire [2:0]                           m_axi_gmem_AWPROT,
    output wire [3:0]                           m_axi_gmem_AWQOS,
    output wire [3:0]                           m_axi_gmem_AWREGION,
    output wire                                 m_axi_gmem_WVALID,
    input  wire                                 m_axi_gmem_WREADY,
    output wire [C_M_AXI_GMEM_DATA_WIDTH-1:0]   m_axi_gmem_WDATA,
    output wire [C_M_AXI_GMEM_DATA_WIDTH/8-1:0] m_axi_gmem_WSTRB,
    output wire                                 m_axi_gmem_WLAST,
    input  wire                                 m_axi_gmem_BVALID,
    output wire                                 m_axi_gmem_BREADY,
    input  wire [1:0]                           m_axi_gmem_BRESP,
    input  wire [C_M_AXI_GMEM_ID_WIDTH - 1:0]   m_axi_gmem_BID,

    dma_write_interface.at_dma DMA_write
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

    // *************************************************************************
    //
    //   Send FIFO
    //
    // *************************************************************************
    logic m_axis_tvalid;
    logic m_axis_tready;
    logic [CLDATA_WIDTH-1:0] m_axis_tdata;
    normal2axis_fifo
    #(.FIFO_WIDTH(CLDATA_WIDTH), .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    send_fifo
    (
        .clk(clk),
        .resetn(!reset),
        .write_enable(DMA_write.tx_write.we),
        .write_data(DMA_write.tx_write.wdata),
        .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready),
        .m_axis_tdata(m_axis_tdata),
        .almostfull(DMA_write.rx_write.walmostfull),
        .count()
    );

    assign DMA_write.status.idle = (send_state == STATE_IDLE);
    assign DMA_write.status.active = (send_state == STATE_WRITE);

    ///////////////////////////////////////////////////////////////////////////////
    // RTL Logic 
    ///////////////////////////////////////////////////////////////////////////////
    // Tie-off unused AXI protocol features
    assign m_axi_gmem_AWID     = {C_M_AXI_GMEM_ID_WIDTH{1'b0}};
    assign m_axi_gmem_AWBURST  = 2'b01;
    assign m_axi_gmem_AWLOCK   = 2'b00;
    assign m_axi_gmem_AWCACHE  = 4'b0011;
    assign m_axi_gmem_AWPROT   = 3'b000;
    assign m_axi_gmem_AWQOS    = 4'b0000;
    assign m_axi_gmem_AWREGION = 4'b0000;

    localparam integer LP_DW_BYTES = CLDATA_WIDTH/8;
    localparam integer LP_AXI_BURST_LEN = 4096/LP_DW_BYTES < 256 ? 4096/LP_DW_BYTES : 256;
    localparam integer LP_LOG_BURST_LEN = $clog2(LP_AXI_BURST_LEN);

    logic krnl_ctrl_start;
    logic krnl_ctrl_done;
    logic [CLADDR_WIDTH-1:0] krnl_ctrl_addr;
    logic [31:0] krnl_ctrl_length;
    krnl_axi_write_master #( 
        .C_ADDR_WIDTH       (CLADDR_WIDTH),
        .C_DATA_WIDTH       (CLDATA_WIDTH),
        .C_MAX_LENGTH_WIDTH (32) ,
        .C_BURST_LEN        (LP_AXI_BURST_LEN),
        .C_LOG_BURST_LEN    (LP_LOG_BURST_LEN)
    )
    inst_axi_write_master ( 
        .aclk        (clk),
        .areset      (reset),

        .ctrl_start  (krnl_ctrl_start),
        .ctrl_offset (krnl_ctrl_addr),
        .ctrl_length (krnl_ctrl_length),
        .ctrl_done   (krnl_ctrl_done),

        .awvalid     (m_axi_gmem_AWVALID),
        .awready     (m_axi_gmem_AWREADY),
        .awaddr      (m_axi_gmem_AWADDR),
        .awlen       (m_axi_gmem_AWLEN),
        .awsize      (m_axi_gmem_AWSIZE),

        .s_tvalid    (m_axis_tvalid),
        .s_tready    (m_axis_tready),
        .s_tdata     (m_axis_tdata),

        .wvalid      (m_axi_gmem_WVALID),
        .wready      (m_axi_gmem_WREADY),
        .wdata       (m_axi_gmem_WDATA),
        .wstrb       (m_axi_gmem_WSTRB),
        .wlast       (m_axi_gmem_WLAST),

        .bvalid      (m_axi_gmem_BVALID),
        .bready      (m_axi_gmem_BREADY),
        .bresp       (m_axi_gmem_BRESP)
    );

    // *************************************************************************
    //
    //   Counter
    //
    // *************************************************************************
    logic [31:0] num_sent_lines;
    logic [31:0] num_ack_lines;

    always_ff @(posedge clk)
    begin
        krnl_ctrl_start <= 1'b0;
        DMA_write.rx_write.wvalid <= 1'b0;
        DMA_write.status.done <= 1'b0;

        case(send_state)
            STATE_IDLE:
            begin
                if (DMA_write.control.start)
                begin
                    krnl_ctrl_start <= 1'b1;
                    // *************************************************************************
                    krnl_ctrl_addr <= DMA_write.control.addr;
                    krnl_ctrl_length <= DMA_write.control.regs.reg4;
                    // *************************************************************************
                    num_sent_lines <= 0;
                    num_ack_lines <= 0;
                    send_state <= STATE_WRITE;
                end
            end

            STATE_WRITE:
            begin
                if (DMA_write.tx_write.we)
                begin
                    num_sent_lines <= num_sent_lines + 1;
                end

                if (m_axi_gmem_BVALID)
                begin
                    DMA_write.rx_write.wvalid <= 1'b1;
                    num_ack_lines <= num_ack_lines + 1;
                end

                if (krnl_ctrl_done)
                begin
                    send_state <= STATE_DONE;
                end
            end

            STATE_DONE:
            begin
                DMA_write.status.done <= 1'b1;
                send_state <= STATE_IDLE;
            end

        endcase

        if (reset)
        begin
            send_state <= STATE_IDLE;
        end
    end

endmodule // pipearch_dma_write_xilinx