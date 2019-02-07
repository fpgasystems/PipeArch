`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"
`include "pipearch_common.vh"
`include "glm_common.vh"

module glm_top
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
    t_ccip_vc vc_select;

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
                program_length <= csrs.cpu_wr_csrs[3].data[15:0];
                vc_select <= t_ccip_vc'(csrs.cpu_wr_csrs[3].data[17:16]);
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

    always_ff @(posedge clk)
    begin
        af2cp_sTx.c0.hdr <= t_cci_c0_ReqMemHdr'(0);
        af2cp_sTx.c0.valid <= 1'b0;

        if (reset)
        begin
            request_state <= RXTX_STATE_IDLE;
            receive_state <= RXTX_STATE_IDLE;
            program_access.we <= 1'b0;
        end
        else
        begin
            // =================================
            //
            //   Request State Machine
            //
            // =================================
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
                        af2cp_sTx.c0.hdr.vc_sel <= vc_select;
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
                        af2cp_sTx.c0.hdr.vc_sel <= vc_select;
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
    //
    //   Write Back
    //
    // =========================================================================
    always_ff @(posedge clk)
    begin
        af2cp_sTx.c1.hdr <= t_cci_c1_ReqMemHdr'(0);
        af2cp_sTx.c1.hdr.sop <= 1'b1;
        af2cp_sTx.c1.valid <= 1'b0;

        if (receive_state == RXTX_STATE_PROGRAM_EXECUTE)
        begin
            execute_writeback_cp2af_sRx_c1 <= cp2af_sRx.c1;
            execute_writeback_c1TxAlmFull <= cp2af_sRx.c1TxAlmFull;
            af2cp_sTx.c1 <= execute_writeback_af2cp_sTx_c1;
            af2cp_sTx.c1.hdr.vc_sel <= vc_select;
        end
        else if (receive_state == RXTX_STATE_DONE)
        begin
            af2cp_sTx.c1.valid <= 1'b1;
            af2cp_sTx.c1.data <= t_ccip_clData'(64'h1);
            af2cp_sTx.c1.hdr.address <= out_addr;
            af2cp_sTx.c1.hdr.vc_sel <= vc_select;
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

    // if opcode == 0xN0 ---- Inrement PC
    //  programCounter++

    // if opcode == 0xN1 ---- jump0
    //  if reg[0] == instruction[13]:
    //      programCounter = instruction[14][15:0]
    //  else:
    //      programCounter = instruction[14][31:16]

    // if opcode == 0xN2 ---- jump1
    //  if reg[1] == instruction[13]:
    //      programCounter = instruction[14][15:0]
    //  else:
    //      programCounter = instruction[14][31:16]

    // if opcode == 0xN3 ---- jump3
    //  if reg[2] == instruction[13]:
    //      programCounter = instruction[14][15:0]
    //  else:
    //      programCounter = instruction[14][31:16]

    // if opcode == 0x1N ---- prefetch
    // reg[3] = instruction[3]+reg[2]*instruction[12]+reg[1]*instruction[11]+reg[0]*instruction[10]     // DRAM read offset in cachelines
                                                                                                        // instruction[10]: read offset change per index0
                                                                                                        // instruction[11]: read offset change per index1
                                                                                                        // instruction[12]: read offset change per index2
    // reg[4] = instruction[4]                                                                          // DRAM read length in cachelines

    // if opcode == 0x2N ---- load
    // reg[3] = instruction[3]+reg[2]*instruction[12]+reg[1]*instruction[11]+reg[0]*instruction[10]     // DRAM read offset in cachelines
                                                                                                        // instruction[10]: read offset change per index0
                                                                                                        // instruction[11]: read offset change per index1
                                                                                                        // instruction[12]: read offset change per index2
    // reg[4] = instruction[4]                                                                          // DRAM read length in cachelines
    // reg[x] = instruction[x]                                                                          // [15:0]: memory store offset in cachelines
                                                                                                        // [31:16]: memory/fifo store length in cachelines

    // if opcode == 0x3N ---- writeback
    // reg[3] = instruction[3]+reg[2]*instruction[12]+reg[1]*instruction[11]+reg[0]*instruction[10]     // DRAM store offset in cachelines
                                                                                                        // [31] DRAM buffer (0 out) (1 in)
    // reg[4] = instruction[4]                                                                          // DRAM store length in cachelines
    // reg[5] = instruction[5]                                                                          // [3:0] Internal read channel select
                                                                                                        // [4] Write Fence
    // reg[x] = instruction[x]                                                                          // [15:0]: memory load offset in cachelines
                                                                                                        // [31:16]: memory/fifo load length in cachelines

    // if opcode == 0x4N ---- dot
    // reg[3] = instruction[3]                                                  // [15:0] Read length in cachelines
                                                                                // [16] Read from modelforward
                                                                                // [17] Perform subtraction
    // reg[4] = instruction[4]                                                  // [15:0] model memory load offset in cachelines
                                                                                // [31:16] label memory load offset in cachelines

    // if opcode == 0x5N ---- modify
    // reg[3] = instruction[3]                                                  // [15:0]: memory2 load offset in cachelines
                                                                                // [31:16]: memory2 store offset in cachelines
    // reg[4] = instruction[4]                                                  // [1:0]: (0 linreg) (1 logreg) (2 SVM)
                                                                                // [2]: (0 SGD) (1 SCD)
    // reg[5] = instruction[5]                                                  // step size
    // reg[6] = instruction[6]                                                  // lambda

    // if opcode == 0x6N ---- update
    // reg[3] = instruction[3]                                                  // [15:0] memory1 load/store offset in cacheline
                                                                                // [31:16] memory1 load/store length in cachelines
    // reg[4] = instruction[4]                                                  // [0] write to model_forward_fifo


    logic [15:0] program_counter;
    logic [31:0] instruction [16];
    logic [31:0] regs [NUM_REGS];
    logic [7:0] opcode;
    logic nonblocking;

    logic [5:0] op_start;
    logic [5:0] op_done;

    logic [31:0] prefetch_regs [5];
    logic [31:0] load_regs [5+NUM_LOAD_CHANNELS];
    logic [31:0] writeback_regs [6+NUM_WRITEBACK_CHANNELS];

    logic [31:0] dot_regs [5];
    logic [31:0] modify_regs [7];
    logic [31:0] update_regs [5];

    always_comb
    begin
        for (int i=0; i < 16; i=i+1)
        begin
            instruction[i] = program_access.rdata[ (i*32)+31 -: 32 ];
        end
    end

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
                        program_access.re <= 1'b1;
                        program_access.raddr <= program_counter;
                        machine_state <= MACHINE_STATE_INSTRUCTION_DECODE;
                    end
                end

                MACHINE_STATE_INSTRUCTION_DECODE:
                begin

                    if (program_access.rvalid)
                    begin
                        opcode <= instruction[15][7:0];
                        nonblocking <= instruction[15][8];
                        case(instruction[15][7:4])

                            4'h1: // prefetch
                            begin
                                op_start[0] <= 1'b1;
                                prefetch_regs[0] <= regs[0]*instruction[10];
                                prefetch_regs[1] <= regs[1]*instruction[11];
                                prefetch_regs[2] <= regs[2]*instruction[12];
                                prefetch_regs[3] <= instruction[3]; // read offset
                                prefetch_regs[4] <= instruction[4]; // read length in cachelines
                            end

                            4'h2: // load
                            begin
                                op_start[1] <= 1'b1;
                                load_regs[0] <= regs[0]*instruction[10];
                                load_regs[1] <= regs[1]*instruction[11];
                                load_regs[2] <= regs[2]*instruction[12];
                                load_regs[3] <= instruction[3]; // read offset
                                load_regs[4] <= instruction[4]; // read length in cachelines
                                for (int i = 0; i < NUM_LOAD_CHANNELS; i++)
                                begin
                                    load_regs[5+i] <= instruction[5+i]; 
                                end
                            end

                            4'h3: // writeback
                            begin
                                op_start[2] <= 1'b1;
                                writeback_regs[0] <= regs[0]*instruction[10];
                                writeback_regs[1] <= regs[1]*instruction[11];
                                writeback_regs[2] <= regs[2]*instruction[12];
                                writeback_regs[3] <= instruction[3]; // store offset
                                writeback_regs[4] <= instruction[4]; // store length in cachelines
                                writeback_regs[5] <= instruction[5]; // channel select
                                for (int i = 0; i < NUM_WRITEBACK_CHANNELS; i++)
                                begin
                                    writeback_regs[6+i] <= instruction[6+i]; 
                                end
                            end

                            // *************************************************************************
                            //
                            //   Additional opcodes
                            //
                            // *************************************************************************
                            4'h4: // dot
                            begin
                                op_start[3] <= 1'b1;
                                dot_regs[0] <= regs[0];
                                dot_regs[1] <= regs[1];
                                dot_regs[2] <= regs[2];
                                dot_regs[3] <= instruction[3];
                                dot_regs[4] <= instruction[4];
                            end

                            4'h5: // modify
                            begin
                                op_start[4] <= 1'b1;
                                modify_regs[0] <= regs[0];
                                modify_regs[1] <= regs[1];
                                modify_regs[2] <= regs[2];
                                modify_regs[3] <= instruction[3];
                                modify_regs[4] <= instruction[4];
                                modify_regs[5] <= instruction[5];
                                modify_regs[6] <= instruction[6];
                            end

                            4'h6: // update
                            begin
                                op_start[5] <= 1'b1;
                                update_regs[0] <= regs[0];
                                update_regs[1] <= regs[1];
                                update_regs[2] <= regs[2];
                                update_regs[3] <= instruction[3];
                                update_regs[4] <= instruction[4];
                            end
                        endcase

                        case(instruction[15][3:0])
                            4'h0:
                            begin
                                program_counter <= program_counter + 1;
                            end

                            4'h1:
                            begin
                                program_counter <= (regs[0] == instruction[13]) ? instruction[14][15:0] : instruction[14][31:16];
                            end

                            4'h2:
                            begin
                                program_counter <= (regs[1] == instruction[13]) ? instruction[14][15:0] : instruction[14][31:16];
                            end

                            4'h3:
                            begin
                                program_counter <= (regs[2] == instruction[13]) ? instruction[14][15:0] : instruction[14][31:16];
                            end
                        endcase

                        machine_state <= MACHINE_STATE_EXECUTE;
                    end
                end

                MACHINE_STATE_EXECUTE:
                begin
                    if (program_counter == 16'hFFFF)
                    begin
                        machine_state <= MACHINE_STATE_DONE;
                    end
                    else if (op_done[opcode[7:4]-1] || opcode[7:4] == 4'b0 || nonblocking)
                    begin
                        program_access.re <= 1'b1;
                        program_access.raddr <= program_counter;
                        machine_state <= MACHINE_STATE_INSTRUCTION_DECODE;
                        regs[0] <= updateIndex(instruction[0], regs[0]);
                        regs[1] <= updateIndex(instruction[1], regs[1]);
                        regs[2] <= updateIndex(instruction[2], regs[2]);
                    end
                end

                MACHINE_STATE_DONE:
                begin
                    program_counter <= 0;
                    machine_state <= MACHINE_STATE_IDLE;
                end
            endcase

        end
    end

    // *************************************************************************
    //
    //   Local Memories
    //
    // *************************************************************************

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_model_interface();
    bram
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE))
    MEM_model (
        .clk,
        .access(MEM_model_interface.bram_source)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_labels_interface();
    bram
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE))
    MEM_labels (
        .clk,
        .access(MEM_labels_interface.bram_source)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_accessprops_interface();
    bram
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE))
    MEM_accessprops (
        .clk,
        .access(MEM_accessprops_interface.bram_source)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE)) FIFO_input_interface();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_SIZE))
    FIFO_input (
        .clk, .reset,
        .access(FIFO_input_interface.fifo_source)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) FIFO_samplesforward_interface();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE))
    FIFO_samplesforward (
        .clk, .reset,
        .access(FIFO_samplesforward_interface.fifo_source)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) FIFO_modelforward_interface();
    fifo
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_INTERNAL_SIZE))
    FIFO_modelforward (
        .clk, .reset,
        .access(FIFO_modelforward_interface.fifo_source)
    );

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) FIFO_dot_interface();
    fifo
    #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE))
    FIFO_dot (
        .clk, .reset,
        .access(FIFO_dot_interface.fifo_source)
    );

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) FIFO_gradient_interface();
    fifo
    #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE))
    FIFO_gradient (
        .clk, .reset,
        .access(FIFO_gradient_interface.fifo_source)
    );


    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) load_MEM_model_interface();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) load_MEM_labels_interface();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) load_MEM_accessprops_interface();

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) dot_MEM_model_interface();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) dot_MEM_labels_interface();

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) modify_MEM_labels_interface();

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) update_MEM_model_interface();

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) writeback_MEM_model_interface();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) writeback_MEM_labels_interface();

    assign load_MEM_accessprops_interface.rvalid = MEM_accessprops_interface.rvalid;
    assign load_MEM_accessprops_interface.rdata = MEM_accessprops_interface.rdata;

    assign dot_MEM_model_interface.rvalid = MEM_model_interface.rvalid;
    assign dot_MEM_model_interface.rdata = MEM_model_interface.rdata;
    assign dot_MEM_labels_interface.rvalid = MEM_labels_interface.rvalid;
    assign dot_MEM_labels_interface.rdata = MEM_labels_interface.rdata;

    assign modify_MEM_labels_interface.rvalid = MEM_labels_interface.rvalid;
    assign modify_MEM_labels_interface.rdata = MEM_labels_interface.rdata;

    assign update_MEM_model_interface.rvalid = MEM_model_interface.rvalid;
    assign update_MEM_model_interface.rdata = MEM_model_interface.rdata;

    assign writeback_MEM_model_interface.rvalid = MEM_model_interface.rvalid;
    assign writeback_MEM_model_interface.rdata = MEM_model_interface.rdata;
    assign writeback_MEM_labels_interface.rvalid = MEM_labels_interface.rvalid;
    assign writeback_MEM_labels_interface.rdata = MEM_labels_interface.rdata;

    always_comb
    begin
        // MEM_model request arbitration
        MEM_model_interface.re <= 1'b0;
        if (dot_MEM_model_interface.re)
        begin
            MEM_model_interface.re <= 1'b1;
            MEM_model_interface.raddr <= dot_MEM_model_interface.raddr;
        end
        else if (update_MEM_model_interface.re)
        begin
            MEM_model_interface.re <= 1'b1;
            MEM_model_interface.raddr <= update_MEM_model_interface.raddr;
        end
        else if (writeback_MEM_model_interface.re)
        begin
            MEM_model_interface.re <= 1'b1;
            MEM_model_interface.raddr <= writeback_MEM_model_interface.raddr;
        end

        // MEM_labels request arbitration
        MEM_labels_interface.re <= 1'b0;
        if (modify_MEM_labels_interface.re)
        begin
            MEM_labels_interface.re <= 1'b1;
            MEM_labels_interface.raddr <= modify_MEM_labels_interface.raddr;
        end
        else if (dot_MEM_labels_interface.re)
        begin
            MEM_labels_interface.re <= 1'b1;
            MEM_labels_interface.raddr <= dot_MEM_labels_interface.raddr;
        end
        else if (writeback_MEM_labels_interface.re)
        begin
            MEM_labels_interface.re <= 1'b1;
            MEM_labels_interface.raddr <= writeback_MEM_labels_interface.raddr;
        end

        // MEM_accessprops request arbitration
        MEM_accessprops_interface.re <= 1'b0;
        if (load_MEM_accessprops_interface.re)
        begin
            MEM_accessprops_interface.re <= 1'b1;
            MEM_accessprops_interface.raddr <= load_MEM_accessprops_interface.raddr;
        end

        // MEM_model write arbitration
        MEM_model_interface.we <= 1'b0;
        if (load_MEM_model_interface.we)
        begin
            MEM_model_interface.we <= 1'b1;
            MEM_model_interface.waddr <= load_MEM_model_interface.waddr;
            MEM_model_interface.wdata <= load_MEM_model_interface.wdata;
        end
        else if (update_MEM_model_interface.we)
        begin
            MEM_model_interface.we <= 1'b1;
            MEM_model_interface.waddr <= update_MEM_model_interface.waddr;
            MEM_model_interface.wdata <= update_MEM_model_interface.wdata;
        end

        // MEM_labels write arbitration
        MEM_labels_interface.we <= 1'b0;
        if (load_MEM_labels_interface.we)
        begin
            MEM_labels_interface.we <= 1'b1;
            MEM_labels_interface.waddr <= load_MEM_labels_interface.waddr;
            MEM_labels_interface.wdata <= load_MEM_labels_interface.wdata;
        end
        else if (modify_MEM_labels_interface.we)
        begin
            MEM_labels_interface.we <= 1'b1;
            MEM_labels_interface.waddr <= modify_MEM_labels_interface.waddr;
            MEM_labels_interface.wdata <= modify_MEM_labels_interface.wdata;
        end

        // MEM_accessprops write arbitration
        MEM_accessprops_interface.we <= 1'b0;
        if (load_MEM_accessprops_interface.we)
        begin
            MEM_accessprops_interface.we <= 1'b1;
            MEM_accessprops_interface.waddr <= load_MEM_accessprops_interface.waddr;
            MEM_accessprops_interface.wdata <= load_MEM_accessprops_interface.wdata;
        end
    end

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
        .regs(prefetch_regs),
        .in_addr,
        .c0TxAlmFull(execute_load_c0TxAlmFull),
        .cp2af_sRx_c0(execute_load_cp2af_sRx_c0),
        .af2cp_sTx_c0(execute_load_af2cp_sTx_c0),
        .get_c0TxAlmFull(execute_afterprefetch_c0TxAlmFull),
        .get_cp2af_sRx_c0(execute_afterprefetch_cp2af_sRx_c0),
        .get_af2cp_sTx_c0(execute_afterprefetch_af2cp_sTx_c0)
    );

    glm_load
    execute_load
    (
        .clk,
        .reset,
        .op_start(op_start[1]),
        .op_done(op_done[1]),
        .regs(load_regs),
        .in_addr,
        .c0TxAlmFull(execute_afterprefetch_c0TxAlmFull),
        .cp2af_sRx_c0(execute_afterprefetch_cp2af_sRx_c0),
        .af2cp_sTx_c0(execute_afterprefetch_af2cp_sTx_c0),
        .FIFO_input(FIFO_input_interface.fifo_write),
        .FIFO_samplesforward(FIFO_samplesforward_interface.fifo_write),
        .MEM_model(load_MEM_model_interface.bram_write),
        .MEM_labels(load_MEM_labels_interface.bram_write),
        .MEM_accessprops(load_MEM_accessprops_interface.bram_readwrite)
    );

    
    glm_writeback
    execute_writeback
    (
        .clk,
        .reset,
        .op_start(op_start[2]),
        .op_done(op_done[2]),
        .regs(writeback_regs),
        .in_addr,
        .out_addr,
        .MEM_model(writeback_MEM_model_interface.bram_read),
        .MEM_labels(writeback_MEM_labels_interface.bram_read),
        .c1TxAlmFull(execute_writeback_c1TxAlmFull),
        .cp2af_sRx_c1(execute_writeback_cp2af_sRx_c1),
        .af2cp_sTx_c1(execute_writeback_af2cp_sTx_c1)
    );

    // *************************************************************************
    //
    //   Local Computation
    //
    // *************************************************************************
    
    glm_dot
    execute_dot
    (
        .clk,
        .reset,
        .op_start(op_start[3]),
        .op_done(op_done[3]),
        .regs(dot_regs),
        .FIFO_input(FIFO_input_interface.fifo_read),
        .MEM_labels(dot_MEM_labels_interface.bram_read),
        .MEM_model(dot_MEM_model_interface.bram_read),
        .FIFO_modelforward(FIFO_modelforward_interface.fifo_read),
        .FIFO_dot(FIFO_dot_interface.fifo_write)
    );

    glm_modify
    execute_modify
    (
        .clk,
        .reset,
        .op_start(op_start[4]),
        .op_done(op_done[4]),
        .regs(modify_regs),
        .MEM_labels(modify_MEM_labels_interface.bram_readwrite),
        .FIFO_dot(FIFO_dot_interface.fifo_read),
        .FIFO_gradient(FIFO_gradient_interface.fifo_write)
    );

    glm_update
    execute_update
    (
        .clk,
        .reset,
        .op_start(op_start[5]),
        .op_done(op_done[5]),
        .regs(update_regs),
        .FIFO_samplesforward(FIFO_samplesforward_interface.fifo_read),
        .FIFO_gradient(FIFO_gradient_interface.fifo_read),
        .MEM_model(update_MEM_model_interface.bram_readwrite),
        .FIFO_modelforward(FIFO_modelforward_interface.fifo_write)
    );

endmodule