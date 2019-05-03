//
// Copyright (c) 2017, Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the Intel Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"

//
// AFU wrapper -- convert MPF interface to CCI-P structures and pass them
//                to the AFU implementation.
//
module intel_top
   (
    input logic clk,
    input logic userclk,

    // Connection toward the host.  Reset comes in here.
    cci_mpf_if.to_fiu fiu,

    // CSR connections
    app_csrs.app csrs,

    // MPF tracks outstanding requests.  These will be true as long as
    // reads or unacknowledged writes are still in flight.
    input  logic c0NotEmpty,
    input  logic c1NotEmpty
    );

    // Local reset to reduce fan-out
    logic reset = 1'b1;
    always @(posedge clk)
    begin
        reset <= fiu.reset;
    end

    //
    // Convert MPF interfaces back to the standard CCI structures.
    //
    t_if_ccip_Rx mpf2af_sRx;
    t_if_ccip_Tx af2mpf_sTx;

    //
    // The base module has already registered the Rx wires heading
    // toward the AFU, so wires are acceptable.
    //
    always_comb
    begin
        //
        // Response wires
        //
        mpf2af_sRx.c0 = fiu.c0Rx;
        mpf2af_sRx.c1 = fiu.c1Rx;

        mpf2af_sRx.c0TxAlmFull = fiu.c0TxAlmFull;
        mpf2af_sRx.c1TxAlmFull = fiu.c1TxAlmFull;

        //
        // Request wires
        //
        fiu.c0Tx = cci_mpf_cvtC0TxFromBase(af2mpf_sTx.c0);
        if (cci_mpf_c0TxIsReadReq(fiu.c0Tx))
        begin
            // Treat all addresses as virtual.  If MPF's VTP isn't
            // enabled this field is ignored and addresses will remain
            // physical.
            fiu.c0Tx.hdr.ext.addrIsVirtual = 1'b1;

            // Enable eVC_VA to physical channel mapping.  This will only
            // be triggered when MPF's ENABLE_VC_MAP is set.
            fiu.c0Tx.hdr.ext.mapVAtoPhysChannel = 1'b1;

            // Enforce load/store and store/store ordering within lines.
            // This will only be triggered when ENFORCE_WR_ORDER is set.
            fiu.c0Tx.hdr.ext.checkLoadStoreOrder = 1'b1;
        end

        fiu.c1Tx = cci_mpf_cvtC1TxFromBase(af2mpf_sTx.c1);
        if (cci_mpf_c1TxIsWriteReq(fiu.c1Tx))
        begin
            // See comments on the c0Tx fields above
            fiu.c1Tx.hdr.ext.addrIsVirtual = 1'b1;
            fiu.c1Tx.hdr.ext.mapVAtoPhysChannel = 1'b1;
            fiu.c1Tx.hdr.ext.checkLoadStoreOrder = 1'b1;

            // Don't ever request an MPF partial write
            fiu.c1Tx.hdr.pwrite = t_cci_mpf_c1_PartialWriteHdr'(0);
        end

        fiu.c2Tx = af2mpf_sTx.c2;
    end

    // Connect to the AFU
    intel_arbiter
    #(.NUM_INSTANCES(2))
    intel_arbiter_inst
    (
        .clk,
        .userclk,
        .reset,
        .cp2af_sRx(mpf2af_sRx),
        .af2cp_sTx(af2mpf_sTx),
        .csrs,
        .c0NotEmpty,
        .c1NotEmpty
    );

    // // For making sure it compiles
    // xilinx_top
    // xilinx_top_inst (
    //     .ap_clk(userclk),
    //     .ap_rst_n(reset),
    //     .m_axi_gmem_AWVALID(),
    //     .m_axi_gmem_AWREADY(),
    //     .m_axi_gmem_AWADDR(),
    //     .m_axi_gmem_AWID(),
    //     .m_axi_gmem_AWLEN(),
    //     .m_axi_gmem_AWSIZE(),
        
    //     .m_axi_gmem_AWBURST(),
    //     .m_axi_gmem_AWLOCK(),
    //     .m_axi_gmem_AWCACHE(),
    //     .m_axi_gmem_AWPROT(),
    //     .m_axi_gmem_AWQOS(),
    //     .m_axi_gmem_AWREGION(),
    //     .m_axi_gmem_WVALID(),
    //     .m_axi_gmem_WREADY(),
    //     .m_axi_gmem_WDATA(),
    //     .m_axi_gmem_WSTRB(),
    //     .m_axi_gmem_WLAST(),
    //     .m_axi_gmem_ARVALID(),
    //     .m_axi_gmem_ARREADY(),
    //     .m_axi_gmem_ARADDR(),
    //     .m_axi_gmem_ARID(),
    //     .m_axi_gmem_ARLEN(),
    //     .m_axi_gmem_ARSIZE(),
    //     .m_axi_gmem_ARBURST(),
    //     .m_axi_gmem_ARLOCK(),
    //     .m_axi_gmem_ARCACHE(),
    //     .m_axi_gmem_ARPROT(),
    //     .m_axi_gmem_ARQOS(),
    //     .m_axi_gmem_ARREGION(),
    //     .m_axi_gmem_RVALID(),
    //     .m_axi_gmem_RREADY(),
    //     .m_axi_gmem_RDATA(),
    //     .m_axi_gmem_RLAST(),
    //     .m_axi_gmem_RID(),
    //     .m_axi_gmem_RRESP(),
    //     .m_axi_gmem_BVALID(),
    //     .m_axi_gmem_BREADY(),
    //     .m_axi_gmem_BRESP(),
    //     .m_axi_gmem_BID(),
    //     .s_axi_control_AWVALID(),
    //     .s_axi_control_AWREADY(),
    //     .s_axi_control_AWADDR(),
    //     .s_axi_control_WVALID(),
    //     .s_axi_control_WREADY(),
    //     .s_axi_control_WDATA(),
    //     .s_axi_control_WSTRB(),
    //     .s_axi_control_ARVALID(),
    //     .s_axi_control_ARREADY(),
    //     .s_axi_control_ARADDR(),
    //     .s_axi_control_RVALID(),
    //     .s_axi_control_RREADY(),
    //     .s_axi_control_RDATA(),
    //     .s_axi_control_RRESP(),
    //     .s_axi_control_BVALID(),
    //     .s_axi_control_BREADY(),
    //     .s_axi_control_BRESP());

endmodule // intel_top