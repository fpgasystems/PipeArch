`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"
`include "pipearch_common.vh"

module pipearch_top
(
    input  logic clk,
    input  logic reset,

    // CCI-P request/response
    input  t_if_ccip_Rx cp2af_sRx,
    output t_if_ccip_Tx af2cp_sTx,

    // CSR connections
    app_csrs.app csrs,

    // MPF tracks outstanding requests.  These will be true as long as
    // reads or unacknowledged writes are still in flight.
    input  logic c0NotEmpty,
    input  logic c1NotEmpty
);

// *************************************************************************
//   FILL: NUM_LOAD_CHANNELS
// *************************************************************************
//?LOAD

// *************************************************************************
//   FILL: NUM_WRITEBACK_CHANNELS
// *************************************************************************
//?WRITEBACK


    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PROGRAM_SIZE)) program_access();
    bram
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_PROGRAM_SIZE))
    program_memory
    (
        .clk,
        .access(program_access.bram_source)
    );

    // =================================
    //
    //   COMMON FUNCTIONS
    //
    // =================================

    //
    // Convert between byte addresses and line addresses.  The conversion
    // is simple: adding or removing low zero bits.
    //
    localparam CL_BYTE_IDX_BITS = 6;
    typedef logic [$bits(t_cci_clAddr) + CL_BYTE_IDX_BITS - 1 : 0] t_byteAddr;

    function automatic t_cci_clAddr byteAddrToClAddr(t_byteAddr addr);
        return addr[CL_BYTE_IDX_BITS +: $bits(t_cci_clAddr)];
    endfunction

    function automatic t_byteAddr clAddrToByteAddr(t_cci_clAddr addr);
        return {addr, CL_BYTE_IDX_BITS'(0)};
    endfunction

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

    t_ccip_clAddr in_addr;
    t_ccip_clAddr out_addr;
    t_ccip_clAddr program_addr;
    logic [15:0] program_length;
    logic start;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            in_addr <= t_ccip_clAddr'(0);
            out_addr <= t_ccip_clAddr'(0);
            program_addr <= t_ccip_clAddr'(0);
            program_length <= 15'b0;
            start <= 1'b0;
        end
        else
        begin
            if (csrs.cpu_wr_csrs[0].en)
            begin
                in_addr <= byteAddrToClAddr(csrs.cpu_wr_csrs[0].data);
            end

            if (csrs.cpu_wr_csrs[1].en)
            begin
                out_addr <= byteAddrToClAddr(csrs.cpu_wr_csrs[1].data);
            end

            if (csrs.cpu_wr_csrs[2].en)
            begin
                program_addr <= byteAddrToClAddr(csrs.cpu_wr_csrs[2].data);
            end

            start <= csrs.cpu_wr_csrs[3].en;
            if (csrs.cpu_wr_csrs[3].en)
            begin
                program_length <= 15'(csrs.cpu_wr_csrs[3].data);
            end

        end
    end

    // =========================================================================
    //
    //   Execute Module Signal Definitions
    //
    // =========================================================================

    logic execute_load_c0TxAlmFull;
    t_if_ccip_c0_Rx execute_load_cp2af_sRx_c0;
    t_if_ccip_c0_Tx execute_load_af2cp_sTx_c0;

    logic execute_writeback_c1TxAlmFull;
    t_if_ccip_c1_Rx execute_writeback_cp2af_sRx_c1;
    t_if_ccip_c1_Tx execute_writeback_af2cp_sTx_c1;

    // =========================================================================
    //
    //   State Definitions
    //
    // =========================================================================

    t_rxtxstate request_state;
    t_rxtxstate receive_state;
    t_machinestate machine_state;

    // =========================================================================
    //
    //   Request/Receive State Machine
    //
    // =========================================================================
    
    logic [15:0] program_length_request;
    logic [15:0] program_length_receive;

    t_cci_c0_ReqMemHdr rd_hdr;
    always_comb
    begin
        rd_hdr = t_cci_c0_ReqMemHdr'(0);
        // Read request type
        rd_hdr.req_type = eREQ_RDLINE_I;
        // Let the FIU pick the channel
        rd_hdr.vc_sel = eVC_VA;
        // Read 4 lines (the size of an entry in the list)
        rd_hdr.cl_len = eCL_LEN_1;
    end

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            request_state <= RXTX_STATE_IDLE;
            receive_state <= RXTX_STATE_IDLE;
            af2cp_sTx.c0.valid <= 1'b0;

            program_access.we <= 1'b0;
        end
        else
        begin
            // =================================
            //
            //   Request State Machine
            //
            // =================================
            af2cp_sTx.c0.valid <= 1'b0;
            case (request_state)
                RXTX_STATE_IDLE:
                begin
                    if (start)
                    begin
                        request_state <= RXTX_STATE_PROGRAM_READ;
                        program_length_request <= 15'b0;
                    end
                end

                RXTX_STATE_PROGRAM_READ:
                begin
                    if (program_length_request < program_length && !cp2af_sRx.c0TxAlmFull)
                    begin
                        af2cp_sTx.c0.valid <= 1'b1;
                        af2cp_sTx.c0.hdr <= rd_hdr;
                        af2cp_sTx.c0.hdr.address <= program_addr + program_length_request;
                        program_length_request <= program_length_request + 1;
                        if (program_length_request == program_length - 1)
                        begin
                            request_state <= RXTX_STATE_PROGRAM_EXECUTE;
                        end
                    end
                end

                RXTX_STATE_PROGRAM_EXECUTE:
                begin
                    if (machine_state == MACHINE_STATE_DONE)
                    begin
                        request_state <= RXTX_STATE_DONE;
                    end
                    else
                    begin
                        af2cp_sTx.c0 <= execute_load_af2cp_sTx_c0;
                    end
                end

                RXTX_STATE_DONE:
                begin
                    request_state <= RXTX_STATE_IDLE;
                end
            endcase

            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            program_access.we <= 1'b0;
            case (receive_state)
                RXTX_STATE_IDLE:
                begin
                    if (start)
                    begin
                        receive_state <= RXTX_STATE_PROGRAM_READ;
                        program_length_receive <= 15'b0;
                    end
                end

                RXTX_STATE_PROGRAM_READ:
                begin
                    if (cci_c0Rx_isReadRsp(cp2af_sRx.c0))
                    begin
                        program_access.we <= 1'b1;
                        program_access.waddr <= program_length_receive;
                        program_access.wdata <= cp2af_sRx.c0.data;
                        program_length_receive <= program_length_receive + 1;
                        if (program_length_receive == program_length-1)
                        begin
                            receive_state <= RXTX_STATE_PROGRAM_EXECUTE;
                        end 
                    end
                end

                RXTX_STATE_PROGRAM_EXECUTE:
                begin
                    if (machine_state == MACHINE_STATE_DONE && !cp2af_sRx.c1TxAlmFull)
                    begin
                        receive_state <= RXTX_STATE_DONE;
                    end
                    else
                    begin
                        execute_load_cp2af_sRx_c0 <= cp2af_sRx.c0;
                        execute_load_c0TxAlmFull <= cp2af_sRx.c0TxAlmFull;
                    end
                end

                RXTX_STATE_DONE:
                begin
                    receive_state <= RXTX_STATE_IDLE;
                end
            endcase
        end
    end

    // =========================================================================
    
    //   Write Back
    
    // =========================================================================
    t_cci_c1_ReqMemHdr wr_hdr;

    always_comb
    begin
        wr_hdr = t_cci_c1_ReqMemHdr'(0);
        // Write request type
        wr_hdr.req_type = eREQ_WRLINE_I;
        // Let the FIU pick the channel
        wr_hdr.vc_sel = eVC_VA;
        // Write 1 line
        wr_hdr.cl_len = eCL_LEN_1;
        // Start of packet is true (single line write)
        wr_hdr.sop = 1'b1;
    end

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            af2cp_sTx.c1.valid <= 1'b0;
        end
        else
        begin
            af2cp_sTx.c1.valid <= 1'b0;

            if (receive_state == RXTX_STATE_PROGRAM_EXECUTE)
            begin
                execute_writeback_cp2af_sRx_c1 <= cp2af_sRx.c1;
                execute_writeback_c1TxAlmFull <= cp2af_sRx.c1TxAlmFull;
                af2cp_sTx.c1 <= execute_writeback_af2cp_sTx_c1;
            end
            else if (receive_state == RXTX_STATE_DONE)
            begin
                af2cp_sTx.c1.valid <= 1'b1;
                af2cp_sTx.c1.data <= t_ccip_clData'(64'h1);
                af2cp_sTx.c1.hdr <= wr_hdr;
                af2cp_sTx.c1.hdr.address <= out_addr;
            end
        end
    end

    //
    // This AFU never handles MMIO reads.  MMIO is managed in the CSR module.
    //
    assign af2cp_sTx.c2.mmioRdValid = 1'b0;

    // =========================================================================
    //
    //   Register Machine
    //
    // =========================================================================

    function automatic logic[31:0] updateIndex(logic[31:0] instruction, logic[31:0] regs);
        logic[31:0] result;
        case(instruction)
            32'hFFFFFFFF:
            begin
                result = regs;
            end

            32'h0FFFFFFF:
            begin
                result = regs + 1;
            end

            32'h01FFFFFF:
            begin
                result = regs - 1;
            end

            default:
            begin
                result = instruction;
            end
        endcase
        return result;
    endfunction

    // register file
    // reg[0]: index0
    // reg[1]: index1
    // reg[2]: index2

    //  if instruction[0,1,2] == 0xFFFFFFFF
    //      reg[0,1,2] = reg[0,1,2]
    //  else if instruction[0,1,2] == 0xFFFFFFF
    //      reg[0,1,2] = reg[0,1,2]+1
    //  else if instruction[0,1,2] == 0x1FFFFFF
    //      reg[0,1,2] = reg[0,1,2]-1
    //  else
    //      reg[0,1,2] = instruction[0,1,2]

    // ----  ISA
    // opcode = instruction[15][7:0]
    // nonblocking = instruction[15][8]

    // if opcode == 0 ---- jump0
    //  if reg[0] == instruction[12]:
    //      programCounter = instruction[13]
    //  else:
    //      programCounter = instruction[14]

    // if opcode == 1 ---- jump1
    //  if reg[1] == instruction[12]:
    //      programCounter = instruction[13]
    //  else:
    //      programCounter = instruction[14]

    // if opcode == 2 ---- jump3
    //  if reg[2] == instruction[12]:
    //      programCounter = instruction[13]
    //  else:
    //      programCounter = instruction[14]


    // if opcode == 10 ---- prefetch
    // reg[3] = instruction[3]+reg[2]*instruction[12]+reg[1]*instruction[11]+reg[0]*instruction[10]     // DRAM read offset in cachelines
                                                                                                        // instruction[10]: read offset change per index0
                                                                                                        // instruction[11]: read offset change per index1
                                                                                                        // instruction[12]: read offset change per index2
    // reg[4] = instruction[4]                                                                          // DRAM read length in cachelines

    // if opcode == 11 ---- load
    // reg[3] = instruction[3]+reg[2]*instruction[12]+reg[1]*instruction[11]+reg[0]*instruction[10]     // DRAM read offset in cachelines
                                                                                                        // instruction[10]: read offset change per index0
                                                                                                        // instruction[11]: read offset change per index1
                                                                                                        // instruction[12]: read offset change per index2
    // reg[4] = instruction[4]                                                                          // DRAM read length in cachelines
    // reg[x] = instruction[x]                                                                          // [15:0]: memory store offset in cachelines
                                                                                                        // [31:16]: memory/fifo store length in cachelines

    // if opcode == 12 ---- writeback
    // reg[3] = instruction[3]+reg[2]*instruction[12]+reg[1]*instruction[11]+reg[0]*instruction[10]     // DRAM store offset in cachelines
                                                                                                        // [31] DRAM buffer (0 out) (1 in)
    // reg[4] = instruction[4]                                                                          // DRAM store length in cachelines
    // reg[5] = instruction[5]                                                                          // Internal read channel select
    // reg[x] = instruction[x]                                                                          // [15:0]: memory load offset in cachelines
                                                                                                        // [31:16]: memory/fifo load length in cachelines


    logic [15:0] program_counter;
    logic [31:0] instruction [16];
    logic [31:0] regs [NUM_REGS];
    logic [7:0] opcode;
    logic nonblocking;

    logic [5:0] op_start;
    logic [5:0] op_done;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            program_counter <= 32'h0;
            machine_state <= MACHINE_STATE_IDLE;
            op_start <= 6'b0;
            opcode <= 8'b0;
            nonblocking <= 1'b0;
            program_access.re <= 1'b0;
        end
        else
        begin
            op_start <= 6'b0;
            program_access.re <= 1'b0;

            case(machine_state)
                MACHINE_STATE_IDLE:
                begin
                    {<<{regs}} <= REGS_WIDTH'(0);
                    if (receive_state == RXTX_STATE_PROGRAM_EXECUTE)
                    begin
                        machine_state <= MACHINE_STATE_INSTRUCTION_FETCH;
                    end
                end

                MACHINE_STATE_INSTRUCTION_FETCH:
                begin
                    program_access.re <= 1'b1;
                    program_access.raddr <= program_counter;
                    machine_state <= MACHINE_STATE_INSTRUCTION_RECEIVE;
                end

                MACHINE_STATE_INSTRUCTION_RECEIVE:
                begin
                    if (program_access.rvalid)
                    begin
                        for (int i=0; i < 16; i=i+1)
                        begin
                            instruction[i] <= program_access.rdata[ (i*32)+31 -: 32 ];
                        end
                        machine_state <= MACHINE_STATE_INSTRUCTION_DECODE;
                    end
                end

                MACHINE_STATE_INSTRUCTION_DECODE:
                begin
                    machine_state <= MACHINE_STATE_EXECUTE;

                    opcode <= instruction[15][7:0];
                    nonblocking <= instruction[15][8];
                    case(instruction[15][7:0])
                        8'h0: // Jump0
                        begin
                            program_counter <= (regs[0] == instruction[12]) ? instruction[13] : instruction[14];
                        end

                        8'h1:
                        begin
                            program_counter <= (regs[1] == instruction[12]) ? instruction[13] : instruction[14];
                        end

                        8'h2:
                        begin
                            program_counter <= (regs[2] == instruction[12]) ? instruction[13] : instruction[14];
                        end

                        8'hA: // prefetch
                        begin
                            op_start[0] <= 1'b1;
                            regs[3] <= instruction[3] + regs[2]*instruction[12] + regs[1]*instruction[11] + regs[0]*instruction[10]; // read offset
                            regs[4] <= instruction[4]; // read length in cachelines
                            program_counter <= program_counter + 1;
                        end

                        8'hB: // load
                        begin
                            op_start[1] <= 1'b1;
                            regs[3] <= instruction[3] + regs[2]*instruction[12] + regs[1]*instruction[11] + regs[0]*instruction[10]; // read offset
                            regs[4] <= instruction[4]; // read length in cachelines
                            for (int i = 0; i < NUM_LOAD_CHANNELS; i++)
                            begin
                                regs[5+i] <= instruction[5+i]; 
                            end
                            program_counter <= program_counter + 1;
                        end

                        8'hC: // writeback
                        begin
                            op_start[2] <= 1'b1;
                            regs[3] <= instruction[3] + regs[2]*instruction[12] + regs[1]*instruction[11] + regs[0]*instruction[10]; // store offset
                            regs[4] <= instruction[4]; // store length in cachelines
                            regs[5] <= instruction[5]; // channel select
                            for (int i = 0; i < NUM_WRITEBACK_CHANNELS; i++)
                            begin
                                regs[6+i] <= instruction[6+i]; 
                            end
                            program_counter <= program_counter + 1;
                        end

// *************************************************************************
//   FILL: Additional opcodes
// *************************************************************************
//?OPCODES

                    endcase
                end

                MACHINE_STATE_EXECUTE:
                begin
                    if (program_counter == 16'hFFFF)
                    begin
                        machine_state <= MACHINE_STATE_DONE;
                    end
                    else if (nonblocking == 1'b1 || op_done[opcode-8'hA] || opcode < 8'hA)
                    begin
                        machine_state <= MACHINE_STATE_INSTRUCTION_FETCH;
                        regs[0] <= updateIndex(instruction[0], regs[0]);
                        regs[1] <= updateIndex(instruction[1], regs[1]);
                        regs[2] <= updateIndex(instruction[2], regs[2]);
                    end
                end

                MACHINE_STATE_DONE:
                begin
                    machine_state <= MACHINE_STATE_IDLE;
                end
            endcase

        end
    end

// *************************************************************************
//   FILL: Local Memories
// *************************************************************************
//?LOCALMEM

    // =========================================================================
    //
    //   DRAM Access
    //
    // =========================================================================

    logic execute_afterprefetch_c0TxAlmFull;
    t_if_ccip_c0_Rx execute_afterprefetch_cp2af_sRx_c0;
    t_if_ccip_c0_Tx execute_afterprefetch_af2cp_sTx_c0;

    pipearch_prefetch
    execute_prefetch
    (
        .clk,
        .reset,
        .op_start(op_start[0]),
        .op_done(op_done[0]),
        .regs0(regs[3]),
        .regs1(regs[4]),
        .in_addr,
        .c0TxAlmFull(execute_load_c0TxAlmFull),
        .cp2af_sRx_c0(execute_load_cp2af_sRx_c0),
        .af2cp_sTx_c0(execute_load_af2cp_sTx_c0),
        .get_c0TxAlmFull(execute_afterprefetch_c0TxAlmFull),
        .get_cp2af_sRx_c0(execute_afterprefetch_cp2af_sRx_c0),
        .get_af2cp_sTx_c0(execute_afterprefetch_af2cp_sTx_c0)
    );


    internal_interface #(.WIDTH(512)) from_load();
    pipearch_load
    execute_load
    (
        .clk,
        .reset,
        .op_start(op_start[1]),
        .op_done(op_done[1]),
        .regs0(regs[3]),
        .regs1(regs[4]),
        .in_addr,
        .c0TxAlmFull(execute_afterprefetch_c0TxAlmFull),
        .cp2af_sRx_c0(execute_afterprefetch_cp2af_sRx_c0),
        .af2cp_sTx_c0(execute_afterprefetch_af2cp_sTx_c0),
        .into_write(from_load.to_commonwrite)
    );

// *************************************************************************
//   FILL: Load Channels
// *************************************************************************
//?LOADCH


    internal_interface #(.WIDTH(512)) to_writeback();
// *************************************************************************
//   FILL: Store Channels
// *************************************************************************
//?STORECH
 
    pipearch_writeback
    execute_writeback
    (
        .clk,
        .reset,
        .op_start(op_start[2]),
        .op_done(op_done[2]),
        .regs0(regs[3]),
        .regs1(regs[4]),
        .in_addr,
        .out_addr,
        .outfrom_read(to_writeback.from_commonread),
        .c1TxAlmFull(execute_writeback_c1TxAlmFull),
        .cp2af_sRx_c1(execute_writeback_cp2af_sRx_c1),
        .af2cp_sTx_c1(execute_writeback_af2cp_sTx_c1)
    );

// *************************************************************************
//   FILL: Local Computation
// *************************************************************************
//?LOCALC

endmodule