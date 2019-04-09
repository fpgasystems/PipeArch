`include "pipearch_common.vh"

`default_nettype none

module xilinx_glm_top
#( 
    parameter integer  C_S_AXI_CONTROL_DATA_WIDTH = 32,
    parameter integer  C_S_AXI_CONTROL_ADDR_WIDTH = 6,
    parameter integer  C_M_AXI_GMEM_ID_WIDTH = 1,
    parameter integer  C_M_AXI_GMEM_ADDR_WIDTH = 42,
    parameter integer  C_M_AXI_GMEM_DATA_WIDTH = 512
)
(
    // System signals
    input  wire  ap_clk,
    input  wire  ap_rst_n,
    // AXI4 master interface 
    output wire                                 m_axi_gmem_AWVALID,
    input  wire                                 m_axi_gmem_AWREADY,
    output wire [C_M_AXI_GMEM_ADDR_WIDTH-1:0]   m_axi_gmem_AWADDR,
    output wire [C_M_AXI_GMEM_ID_WIDTH - 1:0]   m_axi_gmem_AWID,
    output wire [7:0]                           m_axi_gmem_AWLEN,
    output wire [2:0]                           m_axi_gmem_AWSIZE,
    // Tie-off AXI4 transaction options that are not being used.
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
    input  wire                                 m_axi_gmem_BVALID,
    output wire                                 m_axi_gmem_BREADY,
    input  wire [1:0]                           m_axi_gmem_BRESP,
    input  wire [C_M_AXI_GMEM_ID_WIDTH - 1:0]   m_axi_gmem_BID,

    // AXI4-Lite slave interface
    input  wire                                    s_axi_control_AWVALID,
    output wire                                    s_axi_control_AWREADY,
    input  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0]   s_axi_control_AWADDR,
    input  wire                                    s_axi_control_WVALID,
    output wire                                    s_axi_control_WREADY,
    input  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0]   s_axi_control_WDATA,
    input  wire [C_S_AXI_CONTROL_DATA_WIDTH/8-1:0] s_axi_control_WSTRB,
    input  wire                                    s_axi_control_ARVALID,
    output wire                                    s_axi_control_ARREADY,
    input  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0]   s_axi_control_ARADDR,
    output wire                                    s_axi_control_RVALID,
    input  wire                                    s_axi_control_RREADY,
    output wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0]   s_axi_control_RDATA,
    output wire [1:0]                              s_axi_control_RRESP,
    output wire                                    s_axi_control_BVALID,
    input  wire                                    s_axi_control_BREADY,
    output wire [1:0]                              s_axi_control_BRESP 
);

    ///////////////////////////////////////////////////////////////////////////////
    // Variables
    ///////////////////////////////////////////////////////////////////////////////
    logic areset = 1'b0;  
    logic ap_start;
    logic ap_start_1d;
    logic ap_start_pulse;
    logic ap_idle;
    logic ap_done;
    logic [63:0] a;
    logic [63:0] b;
    logic [63:0] c;
    logic [63:0] d;
    config_registers config_regs [4];

    // Register and invert reset signal for better timing.
    always @(posedge ap_clk) begin 
        areset <= ~ap_rst_n;
        ap_start_1d <= ap_start;
    end
    assign ap_start_pulse = ap_start & ~ap_start_1d;

    // AXI4-Lite slave
    krnl_control_s_axi #(
        .C_S_AXI_ADDR_WIDTH( C_S_AXI_CONTROL_ADDR_WIDTH ),
        .C_S_AXI_DATA_WIDTH( C_S_AXI_CONTROL_DATA_WIDTH )
    ) 
    inst_krnl_control_s_axi (
        .AWVALID   ( s_axi_control_AWVALID         ) ,
        .AWREADY   ( s_axi_control_AWREADY         ) ,
        .AWADDR    ( s_axi_control_AWADDR          ) ,
        .WVALID    ( s_axi_control_WVALID          ) ,
        .WREADY    ( s_axi_control_WREADY          ) ,
        .WDATA     ( s_axi_control_WDATA           ) ,
        .WSTRB     ( s_axi_control_WSTRB           ) ,
        .ARVALID   ( s_axi_control_ARVALID         ) ,
        .ARREADY   ( s_axi_control_ARREADY         ) ,
        .ARADDR    ( s_axi_control_ARADDR          ) ,
        .RVALID    ( s_axi_control_RVALID          ) ,
        .RREADY    ( s_axi_control_RREADY          ) ,
        .RDATA     ( s_axi_control_RDATA           ) ,
        .RRESP     ( s_axi_control_RRESP           ) ,
        .BVALID    ( s_axi_control_BVALID          ) ,
        .BREADY    ( s_axi_control_BREADY          ) ,
        .BRESP     ( s_axi_control_BRESP           ) ,
        .ACLK      ( ap_clk                        ) ,
        .ARESET    ( areset                        ) ,
        .ACLK_EN   ( 1'b1                          ) ,
        .ap_start  ( ap_start                      ) ,
        .interrupt (                               ) , // Not used
        .ap_ready  ( ap_idle                       ) ,
        .ap_done   ( ap_done                       ) ,
        .ap_idle   ( ap_idle                       ) ,
        .a         ( a ) ,
        .b         ( b ) ,
        .c         ( c ) ,
        .d         ( d ) 
    );
    assign config_regs[0].en = ap_start_pulse;
    assign config_regs[0].data = a;
    assign config_regs[1].en = ap_start_pulse;
    assign config_regs[1].data = b;
    assign config_regs[2].en = ap_start_pulse;
    assign config_regs[2].data = c;
    assign config_regs[3].en = ap_start_pulse;
    assign config_regs[3].data = d;

    dma_read_interface inst_dma_read();
    dma_write_interface inst_dma_write();

    pipearch_dma_read_xilinx #(
        .C_M_AXI_GMEM_ID_WIDTH(C_M_AXI_GMEM_ID_WIDTH),
        .C_M_AXI_GMEM_ADDR_WIDTH(C_M_AXI_GMEM_ADDR_WIDTH),
        .C_M_AXI_GMEM_DATA_WIDTH(C_M_AXI_GMEM_DATA_WIDTH)
    )
    pipearch_dma_read_xilinx_inst (
        .clk(ap_clk),
        .reset(areset),

        .m_axi_gmem_ARVALID,
        .m_axi_gmem_ARREADY,
        .m_axi_gmem_ARADDR,
        .m_axi_gmem_ARID,
        .m_axi_gmem_ARLEN,
        .m_axi_gmem_ARSIZE,
        .m_axi_gmem_ARBURST,
        .m_axi_gmem_ARLOCK,
        .m_axi_gmem_ARCACHE,
        .m_axi_gmem_ARPROT,
        .m_axi_gmem_ARQOS,
        .m_axi_gmem_ARREGION,
        .m_axi_gmem_RVALID,
        .m_axi_gmem_RREADY,
        .m_axi_gmem_RDATA,
        .m_axi_gmem_RLAST,
        .m_axi_gmem_RID,
        .m_axi_gmem_RRESP,

        .DMA_read(inst_dma_read.at_dma)
    );

    glm_top
    glm_top_inst
    (
        .clk(ap_clk),
        .reset(areset),
        .DMA_read(inst_dma_read.to_dma),
        .DMA_write(inst_dma_write.to_dma),
        .config_regs(config_regs),
        .ctrl_idle(ap_idle),
        .ctrl_done(ap_done),
        .synchronize(),
        .synchronize_done()
    );

    pipearch_dma_write_xilinx #(
        .C_M_AXI_GMEM_ID_WIDTH(C_M_AXI_GMEM_ID_WIDTH),
        .C_M_AXI_GMEM_ADDR_WIDTH(C_M_AXI_GMEM_ADDR_WIDTH),
        .C_M_AXI_GMEM_DATA_WIDTH(C_M_AXI_GMEM_DATA_WIDTH)
    )
    pipearch_dma_write_xilinx_inst (
        .clk(ap_clk),
        .reset(areset),

        .m_axi_gmem_AWVALID,
        .m_axi_gmem_AWREADY,
        .m_axi_gmem_AWADDR,
        .m_axi_gmem_AWID,
        .m_axi_gmem_AWLEN,
        .m_axi_gmem_AWSIZE,
        .m_axi_gmem_AWBURST,
        .m_axi_gmem_AWLOCK,
        .m_axi_gmem_AWCACHE,
        .m_axi_gmem_AWPROT,
        .m_axi_gmem_AWQOS,
        .m_axi_gmem_AWREGION,
        .m_axi_gmem_WVALID,
        .m_axi_gmem_WREADY,
        .m_axi_gmem_WDATA,
        .m_axi_gmem_WSTRB,
        .m_axi_gmem_WLAST,
        .m_axi_gmem_BVALID,
        .m_axi_gmem_BREADY,
        .m_axi_gmem_BRESP,
        .m_axi_gmem_BID,

        .DMA_write(inst_dma_write.at_dma)
    );

endmodule // xilinx_glm_top