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
    //
    // This AFU never handles MMIO reads.  MMIO is managed in the CSR module.
    //
    assign af2cp_sTx.c2.mmioRdValid = 1'b0;

    typedef struct packed
    {
        logic rdreq;
        logic rdempty;
        logic valid;
    }
    t_async_access;

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
            
            async_fifo
            #(.FIFO_WIDTH( $bits(csrs.cpu_wr_csrs[index]) ), .FIFO_DEPTH_BITS(LOG2_PREFETCH_SIZE-3), .ACK(0))
            async_fifo_csrs (
                .wrclk(clk),
                .data(csrs.cpu_wr_csrs[index]),
                .wrreq(csrs.cpu_wr_csrs[index].en),
                .rdclk(userclk),
                .rdreq(csrs_access[index].rdreq),
                .q(intermediate_csrs[index]),
                .rdempty(csrs_access[index].rdempty),
                .wrfull()
            );
            always_ff @(posedge userclk)
            begin
                csrs_access[index].rdreq <= 1'b0;
                if (!csrs_access[index].rdempty)
                begin
                    csrs_access[index].rdreq <= 1'b1;
                end
                csrs_access[index].valid <= csrs_access[index].rdreq && !csrs_access[index].rdempty;
                inst_csrs[index] <= intermediate_csrs[index];
                inst_csrs[index].en <= csrs_access[index].valid;
            end
        end
    endgenerate

    // ====================================================================
    //
    //  Transfer reset
    //
    // ====================================================================
    logic reset_d;
    always_ff @(posedge clk)
    begin
        reset_d <= reset;
    end
    logic transfer_reset;
    logic inst_reset;
    always_ff @(posedge userclk)
    begin
        transfer_reset <= reset | reset_d;
        inst_reset <= transfer_reset;
    end

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
    // logic [1:0] current_Tx_c0_1d = 0;
    // logic [1:0] current_Tx_c0_2d = 0;
    t_async_access Tx_c0 [NUM_INSTANCES];
    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_sTx_c0
            assign Tx_c0[index].rdreq = !Tx_c0[index].rdempty && !cp2af_sRx.c0TxAlmFull && current_Tx_c0 == index;
            async_fifo
            #(.FIFO_WIDTH( $bits(inst_af2cp_sTx[index].c0) ), .FIFO_DEPTH_BITS(LOG2_PREFETCH_SIZE), .ACK(1))
            async_fifo_Tx_c0 (
                .wrclk(userclk),
                .data(inst_af2cp_sTx[index].c0),
                .wrreq(inst_af2cp_sTx[index].c0.valid),
                .rdclk(clk),
                .rdreq(Tx_c0[index].rdreq),
                .q(intermediate_af2cp_sTx[index].c0),
                .rdempty(Tx_c0[index].rdempty),
                .wrfull(inst_cp2af_sRx[index].c0TxAlmFull)
            );
            // always_ff @(posedge clk)
            // begin
            //     Tx_c0[index].rdreq <= 1'b0;
            //     if (!Tx_c0[index].rdempty && !cp2af_sRx.c0TxAlmFull && current_Tx_c0 == index)
            //     begin
            //         Tx_c0[index].rdreq <= 1'b1;
            //     end
            //     Tx_c0[index].valid <= Tx_c0[index].rdreq && !Tx_c0[index].rdempty;
            //     current_Tx_c0_1d <= current_Tx_c0;
            //     current_Tx_c0_2d <= current_Tx_c0_1d;
            // end
        end
    endgenerate
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
        if (reset)
        begin
            current_Tx_c0 <= 0;
        end
    end
    always_ff @(posedge clk)
    begin
        // af2cp_sTx.c0 <= intermediate_af2cp_sTx[current_Tx_c0_2d].c0;
        // af2cp_sTx.c0.hdr.mdata[15:14] <= current_Tx_c0_2d;
        // af2cp_sTx.c0.valid <= 1'b0;
        // if (Tx_c0[current_Tx_c0_2d].valid)
        // begin
        //     af2cp_sTx.c0.valid <= 1'b1;
        // end
        af2cp_sTx.c0.hdr <= t_cci_c0_ReqMemHdr'(0);
        af2cp_sTx.c0.valid <= 1'b0;
        
        if (Tx_c0[current_Tx_c0].rdreq && !Tx_c0[current_Tx_c0].rdempty)
        begin
            af2cp_sTx.c0.valid <= 1'b1;
            af2cp_sTx.c0.hdr <= intermediate_af2cp_sTx[current_Tx_c0].c0.hdr;
            af2cp_sTx.c0.hdr.mdata[15:14] <= current_Tx_c0;
        end
    end

    // ====================================================================
    //
    //  Handle write request
    //
    // ====================================================================
    logic [1:0] current_Tx_c1 = 0;
    logic [1:0] current_Tx_c1_1d = 0;
    logic [1:0] current_Tx_c1_2d = 0;
    t_async_access Tx_c1 [NUM_INSTANCES];
    generate
        for (index = 0; index < NUM_INSTANCES; index = index + 1)
        begin: gen_sTx_c1
            async_fifo
            #(.FIFO_WIDTH( $bits(inst_af2cp_sTx[index].c1) ), .FIFO_DEPTH_BITS(LOG2_PREFETCH_SIZE), .ACK(0))
            async_fifo_Tx_c1 (
                .wrclk(userclk),
                .data(inst_af2cp_sTx[index].c1),
                .wrreq(inst_af2cp_sTx[index].c1.valid),
                .rdclk(clk),
                .rdreq(Tx_c1[index].rdreq),
                .q(intermediate_af2cp_sTx[index].c1),
                .rdempty(Tx_c1[index].rdempty),
                .wrfull(inst_cp2af_sRx[index].c1TxAlmFull)
            );
            always_ff @(posedge clk)
            begin
                Tx_c1[index].rdreq <= 1'b0;
                if (!Tx_c1[index].rdempty && !cp2af_sRx.c1TxAlmFull && current_Tx_c1 == index)
                begin
                    Tx_c1[index].rdreq <= 1'b1;
                end
                Tx_c1[index].valid <= Tx_c1[index].rdreq && !Tx_c1[index].rdempty;
            end
        end
    endgenerate
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
        current_Tx_c1_2d <= current_Tx_c1_1d;
        if (reset)
        begin
            current_Tx_c1 <= 0;
            current_Tx_c1_1d <= 0;
            current_Tx_c1_2d <= 0;
        end
    end
    always_ff @(posedge clk)
    begin
        af2cp_sTx.c1.hdr <= t_cci_c1_ReqMemHdr'(0);
        af2cp_sTx.c1.hdr.sop <= 1'b1;
        af2cp_sTx.c1.valid <= 1'b0;

        af2cp_sTx.c1.data <= intermediate_af2cp_sTx[current_Tx_c1_2d].c1.data;
        if (Tx_c1[current_Tx_c1_2d].valid)
        begin
            af2cp_sTx.c1.hdr <= intermediate_af2cp_sTx[current_Tx_c1_2d].c1.hdr;
            af2cp_sTx.c1.hdr.mdata[15:14] <= current_Tx_c1_2d;
            af2cp_sTx.c1.valid <= 1'b1;
        end
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
            async_fifo
            #(.FIFO_WIDTH( $bits(cp2af_sRx.c0) ), .FIFO_DEPTH_BITS(LOG2_PREFETCH_SIZE), .ACK(0))
            async_fifo_Rx_c0 (
                .wrclk(clk),
                .data(cp2af_sRx.c0),
                .wrreq(cp2af_sRx.c0.rspValid && cp2af_sRx.c0.hdr.mdata[15:14] == index),
                .rdclk(userclk),
                .rdreq(Rx_c0[index].rdreq),
                .q(intermediate_cp2af_sRx[index].c0),
                .rdempty(Rx_c0[index].rdempty),
                .wrfull()
            );
            always_ff @(posedge userclk)
            begin
                Rx_c0[index].rdreq <= 1'b0;
                if (!Rx_c0[index].rdempty)
                begin
                    Rx_c0[index].rdreq <= 1'b1;
                end
                Rx_c0[index].valid <= Rx_c0[index].rdreq && !Rx_c0[index].rdempty;
                inst_cp2af_sRx[index].c0 <= intermediate_cp2af_sRx[index].c0;
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
            async_fifo
            #(.FIFO_WIDTH( $bits(cp2af_sRx.c1) ), .FIFO_DEPTH_BITS(LOG2_PREFETCH_SIZE), .ACK(0))
            async_fifo_Rx_c1 (
                .wrclk(clk),
                .data(cp2af_sRx.c1),
                .wrreq(cp2af_sRx.c1.rspValid && cp2af_sRx.c1.hdr.mdata[15:14] == index),
                .rdclk(userclk),
                .rdreq(Rx_c1[index].rdreq),
                .q(intermediate_cp2af_sRx[index].c1),
                .rdempty(Rx_c1[index].rdempty),
                .wrfull()
            );
            always_ff @(posedge userclk)
            begin
                Rx_c1[index].rdreq <= 1'b0;
                if (!Rx_c1[index].rdempty)
                begin
                    Rx_c1[index].rdreq <= 1'b1;
                end
                Rx_c1[index].valid <= Rx_c1[index].rdreq && !Rx_c1[index].rdempty;
                inst_cp2af_sRx[index].c1 <= intermediate_cp2af_sRx[index].c1;
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