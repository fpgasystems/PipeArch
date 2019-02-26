`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"
`include "pipearch_common.vh"
`include "glm_common.vh"

module pipearch_top
(
    input logic clk,
    input logic userclk,
    input logic reset,

    // CCI-P request/response
    input  t_if_ccip_Rx cp2af_sRx,
    output t_if_ccip_Tx af2cp_sTx,

    // CSR connections
    app_csrs.app csrs,

    // MPF tracks outstanding requests. These will be true as long as
    // reads or unacknowledged writes are still in flight.
    input logic c0NotEmpty,
    input logic c1NotEmpty
);
    // ====================================================================
    //
    //  Transfer reset
    //
    // ====================================================================
    (* preserve *) logic internal_reset[3:0] = '{1'b1, 1'b1, 1'b1, 1'b1};
    assign inst_reset = internal_reset[3];

    always @(posedge clk)
    begin
        internal_reset[0] <= reset;
    end
    always @(posedge userclk)
    begin
        internal_reset[3:1] <= internal_reset[2:0];
    end

    //
    // This AFU never handles MMIO reads.  MMIO is managed in the CSR module.
    //
    assign af2cp_sTx.c2.mmioRdValid = 1'b0;

    typedef struct packed
    {
        logic rdreq;
        logic rdreq_1d;
        logic rdempty;
        logic valid;
        logic wrfull;
        logic wralmfull;
    } t_async_access;

    // ====================================================================
    //
    //  CSRs (simple connections to the external CSR management engine)
    //
    // ====================================================================
    always_comb
    begin
        // The AFU ID is a unique ID for a given program.  Here we generated
        // one with the "uuidgen" program and stored it in the AFU's JSON file.
        // ASE and synthesis setup scripts automatically invoke afu_json_mgr
        // to extract the UUID into afu_json_info.vh.
        csrs.afu_id = `AFU_ACCEL_UUID;
        // Default
        for (int i = 0; i < NUM_APP_CSRS; i = i + 1)
        begin
            csrs.cpu_rd_csrs[i].data = 64'(0);
        end
    end

    t_cpu_wr_csrs intermediate_csrs[0:NUM_APP_CSRS-1];
    t_cpu_wr_csrs inst_csrs[0:NUM_APP_CSRS-1];
    t_async_access csrs_access[0:NUM_APP_CSRS-1];
    genvar index;
    generate
        for(index = 0; index < NUM_APP_CSRS; index = index + 1)
        begin: gen_csr_cross
            platform_utils_dc_fifo
            #(.DATA_WIDTH( /*$bits(csrs.cpu_wr_csrs[index])*/64 ), .DEPTH_RADIX(LOG2_PREFETCH_SIZE-3))
            async_fifo_csrs (
                .data(csrs.cpu_wr_csrs[index].data),
                .wrreq(csrs.cpu_wr_csrs[index].en),
                .rdreq(csrs_access[index].rdreq),
                .wrclk(clk),
                .rdclk(userclk),
                .aclr(internal_reset[0]),
                .q(intermediate_csrs[index].data),
                .rdusedw(),
                .wrusedw(),
                .rdfull(),
                .rdempty(csrs_access[index].rdempty),
                .wrfull(csrs_access[index].wrfull),
                .wralmfull(),
                .wrempty()
            );
            always_ff @(posedge userclk)
            begin
                csrs_access[index].rdreq <= 1'b0;
                if (!csrs_access[index].rdempty)
                begin
                    csrs_access[index].rdreq <= 1'b1;
                end
                csrs_access[index].valid <= csrs_access[index].rdreq && !csrs_access[index].rdempty;
                inst_csrs[index].data <= intermediate_csrs[index].data;
                inst_csrs[index].en <= csrs_access[index].valid;
            end
        end
    endgenerate

    // ====================================================================
    //
    //  Intermediate signals
    //
    // ====================================================================
    t_if_ccip_Tx intermediate_af2cp_sTx [NUM_INSTANCES];
    t_if_ccip_Tx inst_af2cp_sTx [NUM_INSTANCES];
    t_if_ccip_Rx intermediate_cp2af_sRx [NUM_INSTANCES];
    t_if_ccip_Rx inst_cp2af_sRx [NUM_INSTANCES];

    // ====================================================================
    //
    //  Handle read request
    //
    // ====================================================================
    logic [1:0] current_Tx_c0 = 0;
    logic [1:0] current_Tx_c0_1d = 0;
    always_ff @(posedge clk)
    begin
        for (int i = 1; i < NUM_INSTANCES; i=i+1)
        begin
            if (!Tx_c0[i].rdempty) begin
                current_Tx_c0 <= i;
            end
        end
        if (!Tx_c0[0].rdempty && current_Tx_c0 == NUM_INSTANCES-1) begin
            current_Tx_c0 <= 0;
        end
        for (int i = 1; i < NUM_INSTANCES; i=i+1)
        begin
            if (!Tx_c0[i].rdempty && current_Tx_c0 == i-1) begin
                current_Tx_c0 <= i;
            end
        end
        current_Tx_c0_1d <= current_Tx_c0;
        if (reset)
        begin
            current_Tx_c0 <= 0;
            current_Tx_c0_1d <= 0;
        end
    end
    t_async_access Tx_c0 [NUM_INSTANCES];
    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_sTx_c0
            assign Tx_c0[index].rdreq = !Tx_c0[index].rdempty && !cp2af_sRx.c0TxAlmFull && current_Tx_c0 == index;
            assign inst_cp2af_sRx[index].c0TxAlmFull = Tx_c0[index].wralmfull;
            platform_utils_dc_fifo
            #(
                .DATA_WIDTH( $bits({inst_af2cp_sTx[index].c0.hdr}) ),
                .DEPTH_RADIX(LOG2_PREFETCH_SIZE),
                .ALMOST_FULL_THRESHOLD(PREFETCH_SIZE-16)
            )
            async_fifo_Tx_c0 (
                .data(inst_af2cp_sTx[index].c0.hdr),
                .wrreq(inst_af2cp_sTx[index].c0.valid && !Tx_c0[index].wrfull),
                .rdreq(Tx_c0[index].rdreq),
                .wrclk(userclk),
                .rdclk(clk),
                .aclr(internal_reset[0]),
                .q(intermediate_af2cp_sTx[index].c0.hdr),
                .rdusedw(),
                .wrusedw(),
                .rdfull(),
                .rdempty(Tx_c0[index].rdempty),
                .wrfull(Tx_c0[index].wrfull),
                .wralmfull(Tx_c0[index].wralmfull),
                .wrempty()
            );
            always_ff @(posedge clk)
            begin
                Tx_c0[index].rdreq_1d <= Tx_c0[index].rdreq;
            end
        end
    endgenerate
    always_ff @(posedge clk)
    begin
        af2cp_sTx.c0.valid <= Tx_c0[current_Tx_c0_1d].rdreq_1d;
        af2cp_sTx.c0.hdr <= intermediate_af2cp_sTx[current_Tx_c0_1d].c0.hdr;
        af2cp_sTx.c0.hdr.mdata <= {current_Tx_c0_1d, intermediate_af2cp_sTx[current_Tx_c0_1d].c0.hdr.mdata[13:0]};
    end

    // ====================================================================
    //
    //  Handle write request
    //
    // ====================================================================
    logic [1:0] current_Tx_c1 = 0;
    logic [1:0] current_Tx_c1_1d = 0;
    always_ff @(posedge clk)
    begin
        for (int i = 1; i < NUM_INSTANCES; i=i+1)
        begin
            if (!Tx_c1[i].rdempty) begin
                current_Tx_c1 <= i;
            end
        end
        if (!Tx_c1[0].rdempty && current_Tx_c1 == NUM_INSTANCES-1) begin
            current_Tx_c1 <= 0;
        end
        for (int i = 1; i < NUM_INSTANCES; i=i+1)
        begin
            if (!Tx_c1[i].rdempty && current_Tx_c1 == i-1) begin
                current_Tx_c1 <= i;
            end
        end
        current_Tx_c1_1d <= current_Tx_c1;
        if (reset)
        begin
            current_Tx_c1 <= 0;
            current_Tx_c1_1d <= 0;
        end
    end
    t_async_access Tx_c1 [NUM_INSTANCES];
    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_sTx_c1
            assign Tx_c1[index].rdreq = !Tx_c1[index].rdempty && !cp2af_sRx.c1TxAlmFull && current_Tx_c1 == index;
            assign inst_cp2af_sRx[index].c1TxAlmFull = Tx_c1[index].wralmfull;
            platform_utils_dc_fifo
            #(
                .DATA_WIDTH( $bits({inst_af2cp_sTx[index].c1.hdr, inst_af2cp_sTx[index].c1.data}) ),
                .DEPTH_RADIX(LOG2_PREFETCH_SIZE),
                .ALMOST_FULL_THRESHOLD(PREFETCH_SIZE-16)
            )
            async_fifo_Tx_c1 (
                .data({inst_af2cp_sTx[index].c1.hdr, inst_af2cp_sTx[index].c1.data}),
                .wrreq(inst_af2cp_sTx[index].c1.valid && !Tx_c1[index].wrfull),
                .rdreq(Tx_c1[index].rdreq),
                .wrclk(userclk),
                .rdclk(clk),
                .aclr(internal_reset[0]),
                .q({intermediate_af2cp_sTx[index].c1.hdr, intermediate_af2cp_sTx[index].c1.data}),
                .rdusedw(),
                .wrusedw(),
                .rdfull(),
                .rdempty(Tx_c1[index].rdempty),
                .wrfull(Tx_c1[index].wrfull),
                .wralmfull(Tx_c1[index].wralmfull),
                .wrempty()
            );
            always_ff @(posedge clk)
            begin
                Tx_c1[index].rdreq_1d <= Tx_c1[index].rdreq;
            end
        end
    endgenerate
    always_ff @(posedge clk)
    begin
        af2cp_sTx.c1.valid <= Tx_c1[current_Tx_c1_1d].rdreq_1d;
        af2cp_sTx.c1.data <= intermediate_af2cp_sTx[current_Tx_c1_1d].c1.data;
        af2cp_sTx.c1.hdr <= intermediate_af2cp_sTx[current_Tx_c1_1d].c1.hdr;
        af2cp_sTx.c1.hdr.mdata <= {current_Tx_c1_1d, intermediate_af2cp_sTx[current_Tx_c1_1d].c1.hdr.mdata[13:0]};
    end

    // ====================================================================
    //
    //  Handle read response
    //
    // ====================================================================
    t_async_access Rx_c0 [NUM_INSTANCES];
    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_sRx_c0
            platform_utils_dc_fifo
            #(
                .DATA_WIDTH( $bits({cp2af_sRx.c0.hdr, cp2af_sRx.c0.data}) ),
                .DEPTH_RADIX(LOG2_PREFETCH_SIZE),
                .ALMOST_FULL_THRESHOLD(PREFETCH_SIZE-16)
            )
            async_fifo_Rx_c0 (
                .data({cp2af_sRx.c0.hdr, cp2af_sRx.c0.data}),
                .wrreq(cp2af_sRx.c0.rspValid && cp2af_sRx.c0.hdr.mdata[15:14] == index),
                .rdreq(Rx_c0[index].rdreq),
                .wrclk(clk),
                .rdclk(userclk),
                .aclr(internal_reset[0]),
                .q({intermediate_cp2af_sRx[index].c0.hdr, intermediate_cp2af_sRx[index].c0.data}),
                .rdusedw(),
                .wrusedw(),
                .rdfull(),
                .rdempty(Rx_c0[index].rdempty),
                .wrfull(Rx_c0[index].wrfull),
                .wralmfull(Rx_c0[index].wralmfull),
                .wrempty()
            );
            always_ff @(posedge userclk)
            begin
                Rx_c0[index].rdreq <= 1'b0;
                if (!Rx_c0[index].rdempty)
                begin
                    Rx_c0[index].rdreq <= 1'b1;
                end
                Rx_c0[index].valid <= Rx_c0[index].rdreq && !Rx_c0[index].rdempty;
                inst_cp2af_sRx[index].c0.hdr <= intermediate_cp2af_sRx[index].c0.hdr;
                inst_cp2af_sRx[index].c0.data <= intermediate_cp2af_sRx[index].c0.data;
                inst_cp2af_sRx[index].c0.rspValid <= Rx_c0[index].valid;
            end
        end
    endgenerate

    // ====================================================================
    //
    //  Handle write response
    //
    // ====================================================================
    t_async_access Rx_c1 [NUM_INSTANCES];
    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_sRx_c1
            platform_utils_dc_fifo
            #(
                .DATA_WIDTH( $bits(cp2af_sRx.c1.hdr) ),
                .DEPTH_RADIX(LOG2_PREFETCH_SIZE),
                .ALMOST_FULL_THRESHOLD(PREFETCH_SIZE-16)
            )
            async_fifo_Rx_c1 (
                .data(cp2af_sRx.c1.hdr),
                .wrreq(cp2af_sRx.c1.rspValid && cp2af_sRx.c1.hdr.mdata[15:14] == index),
                .rdreq(Rx_c1[index].rdreq),
                .wrclk(clk),
                .rdclk(userclk),
                .aclr(internal_reset[0]),
                .q(intermediate_cp2af_sRx[index].c1.hdr),
                .rdusedw(),
                .wrusedw(),
                .rdfull(),
                .rdempty(Rx_c1[index].rdempty),
                .wrfull(Rx_c1[index].wrfull),
                .wralmfull(Rx_c1[index].wralmfull),
                .wrempty()
            );
            always_ff @(posedge userclk)
            begin
                Rx_c1[index].rdreq <= 1'b0;
                if (!Rx_c1[index].rdempty)
                begin
                    Rx_c1[index].rdreq <= 1'b1;
                end
                Rx_c1[index].valid <= Rx_c1[index].rdreq && !Rx_c1[index].rdempty;
                inst_cp2af_sRx[index].c1.hdr <= intermediate_cp2af_sRx[index].c1.hdr;
                inst_cp2af_sRx[index].c1.rspValid <= Rx_c1[index].valid;
            end
        end
    endgenerate

    // ====================================================================
    //
    //  Instantiate
    //
    // ====================================================================
    logic [NUM_INSTANCES-1:0] synchronize_compare;
    logic [NUM_INSTANCES-1:0] synchronize;
    logic [NUM_INSTANCES-1:0] synchronize_done;

    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_glm_top
            glm_top
            glm_top_inst
            (
                .clk(userclk),
                .reset(inst_reset),
                .cp2af_sRx(inst_cp2af_sRx[index]),
                .af2cp_sTx(inst_af2cp_sTx[index]),
                .wr_csrs(inst_csrs[index*4 +: 4]),
                .synchronize(synchronize[index]),
                .synchronize_done(synchronize_done[index])
            );
        end
    endgenerate

    always_ff @(posedge userclk)
    begin
        if (inst_csrs[NUM_APP_CSRS-1].en)
        begin
            synchronize_compare <= inst_csrs[NUM_APP_CSRS-1][NUM_INSTANCES-1];
        end

        synchronize_done <= 0;
        if (synchronize == synchronize_compare)
        begin
            for (int i = 0; i < NUM_INSTANCES; i=i+1)
            begin
                synchronize_done[i] <= 1'b1;
            end
        end
    end

endmodule // pipearch_top