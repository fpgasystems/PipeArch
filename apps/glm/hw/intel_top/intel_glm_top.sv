`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"
`include "pipearch_common.vh"

module intel_glm_top
(
    input logic clk,
    input logic reset,

    // CCI-P request/response
    input t_if_ccip_Rx cp2af_sRx,
    output t_if_ccip_Tx af2cp_sTx,

    // CSR connections
    input config_registers config_regs [4],

    output logic synchronize,
    input logic synchronize_done
);

    dma_read_interface inst_dma_read();
    dma_write_interface inst_dma_write();

    pipearch_dma_read
    pipearch_dma_read_inst
    (
        .clk(clk),
        .reset(reset),
        .c0TxAlmFull(cp2af_sRx.c0TxAlmFull),
        .cp2af_sRx_c0(cp2af_sRx.c0),
        .af2cp_sTx_c0(af2cp_sTx.c0),
        .vc_select(t_ccip_vc'(config_regs[0][31:30])),
        .DMA_read(inst_dma_read.at_dma)
    );

    glm_top
    glm_top_inst
    (
        .clk(clk),
        .reset(reset),
        .DMA_read(inst_dma_read.to_dma),
        .DMA_write(inst_dma_write.to_dma),
        .config_regs(config_regs),
        .ctrl_idle(),
        .ctrl_done(),
        .synchronize(synchronize),
        .synchronize_done(synchronize_done)
    );

    pipearch_dma_write
    pipearch_dma_write_inst
    (
        .clk(clk),
        .reset(reset),
        .c1TxAlmFull(cp2af_sRx.c1TxAlmFull),
        .cp2af_sRx_c1(cp2af_sRx.c1),
        .af2cp_sTx_c1(af2cp_sTx.c1),
        .vc_select(t_ccip_vc'(config_regs[0][31:30])),
        .DMA_write(inst_dma_write.at_dma)
    );

endmodule // intel_glm_top