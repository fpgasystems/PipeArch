`include "pipearch_common.vh"
`include "glm_common.vh"

module glm_top
(
    input logic clk,
    input logic reset,

    // request/response
    dma_read_interface.to_dma DMA_read,
    dma_write_interface.to_dma DMA_write,

    // CSR connections
    input config_registers config_regs [4],

    output logic ctrl_idle,
    output logic ctrl_done,
    output logic synchronize,
    input logic synchronize_done
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
    localparam CL_BYTE_IDX_BITS = 6;
    typedef logic [$bits(t_claddr) + CL_BYTE_IDX_BITS - 1 : 0] t_byteAddr;
    function automatic t_claddr byteAddrToClAddr(t_byteAddr addr);
        return addr[CL_BYTE_IDX_BITS +: $bits(t_claddr)];
    endfunction

    typedef enum logic [1:0]
    {
        THREAD_IDLE,
        THREAD_LOAD_PROGRAM,
        THREAD_WAITING,
        THREAD_RUNNING
    } t_thread_status;

    typedef struct packed
    {
        t_thread_status status;
        logic [LOG2_PROGRAM_SIZE-1:0] program_length;
        t_claddr program_addr;
        t_claddr in_addr;
        t_claddr out_addr;
    } t_thread_information;

    typedef logic [31:0] t_reg;

    typedef struct packed
    {
        logic [LOG2_PROGRAM_SIZE-1:0] pc_context_load;
        logic [LOG2_PROGRAM_SIZE-1:0] pc_context_store;
        logic [LOG2_PROGRAM_SIZE-1:0] program_counter;
        t_reg [NUM_REGS-1:0] regs;
    } t_thread_context;

    t_thread_information thread_information;
    logic context_switch;
    t_thread_context thread_context;
    t_thread_context thread_context_to_store;

    always_ff @(posedge clk)
    begin

        if (config_regs[0].en)
        begin
            thread_information.program_length <= config_regs[0].data[LOG2_PROGRAM_SIZE-1:0];
            context_switch <= config_regs[0].data[16];
        end

        if (config_regs[1].en)
        begin
            thread_information.program_addr <= byteAddrToClAddr(config_regs[1].data);
        end

        if (config_regs[2].en)
        begin
            thread_information.in_addr <= byteAddrToClAddr(config_regs[2].data);
        end

        thread_information.status <= THREAD_IDLE;
        if (config_regs[3].en)
        begin
            thread_information.status <= THREAD_LOAD_PROGRAM;
            thread_information.out_addr <= byteAddrToClAddr(config_regs[3].data);
        end

        if (reset)
        begin
            thread_information <= t_thread_information'(0);
        end
    end

    // =========================================================================
    //
    //   Execute Module Signal Definitions
    //
    // =========================================================================
    dma_read_interface DMA_rm_read();
    dma_write_interface DMA_rm_write();

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
    
    logic [LOG2_PROGRAM_SIZE-1:0] program_length_request;
    logic [LOG2_PROGRAM_SIZE-1:0] program_length_receive;

    always_ff @(posedge clk)
    begin
        ctrl_idle <= 1'b0;
        ctrl_done <= 1'b0;
        DMA_read.control.start <= 1'b0;
        DMA_write.control.start <= 1'b0;
        DMA_write.tx_write.we <= 1'b0;

        // =================================
        //
        //   Request State Machine
        //
        // =================================
        case (request_state)
            RXTX_STATE_IDLE:
            begin
                ctrl_idle <= 1'b1;
                if (thread_information.status == THREAD_LOAD_PROGRAM)
                begin
                    request_state <= RXTX_STATE_PROGRAM_READ;
                    program_length_request <= 15'b0;
                end
            end

            RXTX_STATE_PROGRAM_READ:
            begin
                DMA_read.control.start <= 1'b1;
                DMA_read.control.async <= 1'b1;
                DMA_read.control.regs <= t_dma_reg'(0);
                DMA_read.control.regs.reg4 <= thread_information.program_length;
                DMA_read.control.addr <= thread_information.program_addr;
                request_state <= RXTX_STATE_CONTEXT_READ;
            end

            RXTX_STATE_CONTEXT_READ:
            begin
                DMA_read.control.start <= 1'b1;
                DMA_read.control.async <= 1'b1;
                DMA_read.control.regs <= t_dma_reg'(0);
                DMA_read.control.regs.reg4 <= 1;
                DMA_read.control.addr <= thread_information.out_addr;
                request_state <= RXTX_STATE_PROGRAM_EXECUTE;
            end

            RXTX_STATE_PROGRAM_EXECUTE:
            begin
                if (machine_state == MACHINE_STATE_DONE)
                begin
                    request_state <= RXTX_STATE_CONTEXT_WRITE;
                end
                else
                begin
                    DMA_read.control <= DMA_rm_read.control;
                    DMA_read.tx_read <= DMA_rm_read.tx_read;
                    DMA_rm_read.status <= DMA_read.status;
                    DMA_rm_read.rx_read <= DMA_read.rx_read;

                    DMA_write.control <= DMA_rm_write.control;
                    DMA_write.tx_write <= DMA_rm_write.tx_write;
                    DMA_rm_write.status <= DMA_write.status;
                    DMA_rm_write.rx_write <= DMA_write.rx_write;
                end
            end

            RXTX_STATE_CONTEXT_WRITE:
            begin
                if (DMA_write.status.idle)
                begin
                    DMA_write.control.start <= 1'b1;
                    DMA_write.control.async <= 1'b1;
                    DMA_write.control.regs <= t_dma_reg'(0);
                    DMA_write.control.regs.reg4 <= 1;
                    DMA_write.control.addr <= thread_information.out_addr;
                    request_state <= RXTX_STATE_DONE;
                end
            end

            RXTX_STATE_DONE:
            begin
                if (DMA_write.rx_write.wvalid)
                begin
                    ctrl_done <= 1'b1;
                    request_state <= RXTX_STATE_IDLE;
                end
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
                if (thread_information.status == THREAD_LOAD_PROGRAM)
                begin
                    receive_state <= RXTX_STATE_PROGRAM_READ;
                    program_length_receive <= 0;
                end
            end

            RXTX_STATE_PROGRAM_READ:
            begin
                if (DMA_read.rx_read.rvalid)
                begin
                    program_access.we <= 1'b1;
                    program_access.waddr <= program_length_receive;
                    program_access.wdata <= DMA_read.rx_read.rdata;
                    program_length_receive <= program_length_receive + 1;
                    if (program_length_receive == thread_information.program_length-1)
                    begin
                        receive_state <= RXTX_STATE_CONTEXT_READ;
                    end 
                end
            end

            RXTX_STATE_CONTEXT_READ:
            begin
                if (DMA_read.rx_read.rvalid)
                begin
                    thread_context.regs[0] <= DMA_read.rx_read.rdata[63:32];
                    thread_context.regs[1] <= DMA_read.rx_read.rdata[95:64];
                    thread_context.regs[2] <= DMA_read.rx_read.rdata[127:96];
                    thread_context.program_counter <= DMA_read.rx_read.rdata[135:128];
                    thread_context.pc_context_store <= DMA_read.rx_read.rdata[143:136];
                    thread_context.pc_context_load <= DMA_read.rx_read.rdata[151:144];
                    receive_state <= RXTX_STATE_PROGRAM_EXECUTE;
                end
            end

            RXTX_STATE_PROGRAM_EXECUTE:
            begin
                if (machine_state == MACHINE_STATE_DONE)
                begin
                    receive_state <= RXTX_STATE_CONTEXT_WRITE;
                end
            end

            RXTX_STATE_CONTEXT_WRITE:
            begin
                if (DMA_write.status.active && !DMA_write.rx_write.walmostfull)
                begin
                    DMA_write.tx_write.we <= 1'b1;
                    DMA_write.tx_write.wdata <= t_cldata'({thread_context_to_store, 32'b1});
                    receive_state <= RXTX_STATE_DONE;
                end
            end

            RXTX_STATE_DONE:
            begin
                if (DMA_write.rx_write.wvalid)
                begin
                    receive_state <= RXTX_STATE_IDLE;
                end
            end
        endcase

        if (reset)
        begin
            request_state <= RXTX_STATE_IDLE;
            receive_state <= RXTX_STATE_IDLE;
        end
    end

    // =========================================================================
    //
    //   Register Machine
    //
    // =========================================================================
    function automatic logic[31:0] DSP27Mult(logic[31:0] left, logic[31:0] right);
        logic[31:0] result;
        result = left[26:0]*right[26:0];
        return result;
    endfunction 

    function automatic logic[31:0] updateIndex(logic[31:0] instruction, logic[31:0] regs);
        logic[31:0] result;
        case(instruction)
            32'hEFFFFFFF:
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
                if (instruction[31] == 1'b1)
                begin
                    result = regs + instruction[30:0];
                end
                else
                begin
                    result = instruction;
                end
            end
        endcase
        return result;
    endfunction

    function automatic logic[15:0] conditional(logic[31:0] regs, logic[31:0] predicate, logic[15:0] false, logic[15:0] true);
        logic[15:0] result;
        if (predicate[31:30] == 2'b01) // if even
        begin
            result = (regs[0] == 1'b0) ? true : false;
        end
        else // equality
        begin
            result = (regs == predicate) ? true : false;
        end
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
    // enableswitch = instruction[15][9]

    // if opcode == 0xN0 ---- Increment PC
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

    // if opcode == 0x7N ---- copy
    // reg[3] = instruction[3]                                                  // [15:0] memory1 load offset in cacheline
                                                                                // [31:16] memory1 load length in cachelines
    // reg[4] = instruction[4]                                                  // [15:0] memory1 store offset in cacheline
                                                                                // [31:16] memory1 store length in cachelines

    // if opcode == 0x8N ---- synchronize

    logic [31:0] instruction [16];
    logic [LOG2_PROGRAM_SIZE-1:0] program_counter;
    t_reg [NUM_REGS-1:0] regs;
    t_reg [NUM_REGS-1:0] temp_regs ;
    logic [7:0] opcode;
    logic nonblocking;
    logic enableswitch;

    logic [NUM_OPS-1:0] op_start;
    logic [NUM_OPS-1:0] op_done ;
    logic [NUM_OPS-1:0] op_active;

    t_dma_reg prefetch_regs;
    logic [31:0] load_regs [5+NUM_LOAD_CHANNELS];
    logic [31:0] writeback_regs [6+NUM_WRITEBACK_CHANNELS];
    logic [31:0] dot_regs [6];
    logic [31:0] modify_regs [7];
    logic [31:0] update_regs [6];
    logic [31:0] copy_regs [5];

    always_ff @(posedge clk)
    begin
        if(machine_state == MACHINE_STATE_IDLE)
        begin
            op_active <= 0;
        end
        else
        begin
            for (int i=0; i < NUM_OPS; i=i+1)
            begin
                if (op_done[i])
                begin
                    op_active[i] <= 1'b0;
                end
                else if (op_start[i])
                begin
                    op_active[i] <= 1'b1;
                end
            end
        end
    end

    assign synchronize = op_start[7];
    assign op_done[7] = synchronize_done;

    dma_read_interface DMA_load_read();

    // Arbitrate access to dma_read
    assign DMA_rm_read.control.start = op_start[0] | DMA_load_read.control.start;
    assign DMA_rm_read.control.regs = (op_start[0]) ? prefetch_regs : DMA_load_read.control.regs;
    assign DMA_rm_read.control.addr = (op_start[0]) ? thread_information.in_addr : DMA_load_read.control.addr;
    assign DMA_rm_read.control.async = 1'b0;
    assign op_done[0] = DMA_rm_read.status.done;
    assign DMA_load_read.status = DMA_rm_read.status;
    assign DMA_rm_read.tx_read = DMA_load_read.tx_read;
    assign DMA_load_read.rx_read = DMA_rm_read.rx_read;

    always_ff @(posedge clk)
    begin
        op_start <= 0;
        program_access.re <= 1'b0;

        case(machine_state)
            MACHINE_STATE_IDLE:
            begin
                if (receive_state == RXTX_STATE_PROGRAM_EXECUTE)
                begin
                    program_access.re <= 1'b1;
                    if (thread_context.program_counter > 0) // Context has been stored before
                    begin
                        program_counter <= thread_context.pc_context_load;
                        program_access.raddr <= thread_context.pc_context_load;
                    end
                    else
                    begin
                        program_counter <= thread_context.program_counter;
                        program_access.raddr <= thread_context.program_counter;
                    end
                    regs <= thread_context.regs;
                    machine_state <= MACHINE_STATE_INSTRUCTION_RECEIVE;
                end
            end

            MACHINE_STATE_INSTRUCTION_RECEIVE:
            begin
                temp_regs <= regs;
                for (int i=0; i < 16; i=i+1)
                begin
                    instruction[i] <= program_access.rdata[ (i*32)+31 -: 32 ];
                end
                if (program_access.rvalid)
                begin
                    machine_state <= MACHINE_STATE_INSTRUCTION_DECODE;
                end
            end

            MACHINE_STATE_INSTRUCTION_DECODE:
            begin
                if (op_active[instruction[15][7:4]-1] == 1'b0 || instruction[15][7:4] == 0)
                begin
                    opcode <= instruction[15][7:0];
                    nonblocking <= instruction[15][8];
                    enableswitch <= instruction[15][9];
                    case(instruction[15][7:4])

                        4'h1: // prefetch
                        begin
                            op_start[0] <= 1'b1;
                            prefetch_regs.reg0 <= DSP27Mult(temp_regs[0],instruction[10]);
                            prefetch_regs.reg1 <= DSP27Mult(temp_regs[1],instruction[11]);
                            prefetch_regs.reg2 <= DSP27Mult(temp_regs[2],instruction[12]);
                            prefetch_regs.reg3 <= instruction[3]; // read offset
                            prefetch_regs.reg4 <= instruction[4]; // read length in cachelines
                        end

                        4'h2: // load
                        begin
                            op_start[1] <= 1'b1;
                            load_regs[0] <= DSP27Mult(temp_regs[0],instruction[10]);
                            load_regs[1] <= DSP27Mult(temp_regs[1],instruction[11]);
                            load_regs[2] <= DSP27Mult(temp_regs[2],instruction[12]);
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
                            writeback_regs[0] <= DSP27Mult(temp_regs[0],instruction[10]);
                            writeback_regs[1] <= DSP27Mult(temp_regs[1],instruction[11]);
                            writeback_regs[2] <= DSP27Mult(temp_regs[2],instruction[12]);
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
                            dot_regs[0] <= temp_regs[0];
                            dot_regs[1] <= temp_regs[1];
                            dot_regs[2] <= temp_regs[2];
                            dot_regs[3] <= instruction[3];
                            dot_regs[4] <= instruction[4];
                            dot_regs[5] <= instruction[5];
                        end

                        4'h5: // modify
                        begin
                            op_start[4] <= 1'b1;
                            modify_regs[0] <= temp_regs[0];
                            modify_regs[1] <= temp_regs[1];
                            modify_regs[2] <= temp_regs[2];
                            modify_regs[3] <= instruction[3];
                            modify_regs[4] <= instruction[4];
                            modify_regs[5] <= instruction[5];
                            modify_regs[6] <= instruction[6];
                        end

                        4'h6: // update
                        begin
                            op_start[5] <= 1'b1;
                            update_regs[0] <= temp_regs[0];
                            update_regs[1] <= temp_regs[1];
                            update_regs[2] <= temp_regs[2];
                            update_regs[3] <= instruction[3];
                            update_regs[4] <= instruction[4];
                            update_regs[5] <= instruction[5];
                        end

                        4'h7: // copy
                        begin
                            op_start[6] <= 1'b1;
                            copy_regs[0] <= temp_regs[0];
                            copy_regs[1] <= temp_regs[1];
                            copy_regs[2] <= temp_regs[2];
                            copy_regs[3] <= instruction[3];
                            copy_regs[4] <= instruction[4];
                        end

                        4'h8: // synchronize
                        begin
                            op_start[7] <= 1'b1;
                        end

                    endcase

                    case(instruction[15][3:0])
                        4'h0:
                        begin
                            program_counter <= program_counter + 1;
                        end

                        4'h1:
                        begin
                            program_counter <= conditional(temp_regs[0], instruction[13], instruction[14][31:16], instruction[14][15:0]);
                        end

                        4'h2:
                        begin
                            program_counter <= conditional(temp_regs[1], instruction[13], instruction[14][31:16], instruction[14][15:0]);
                        end

                        4'h3:
                        begin
                            program_counter <= conditional(temp_regs[2], instruction[13], instruction[14][31:16], instruction[14][15:0]);
                        end
                    endcase

                    machine_state <= MACHINE_STATE_EXECUTE;
                end
            end

            MACHINE_STATE_CONTEXT_LOAD_DONE:
            begin
                program_counter <= thread_context.program_counter;
                program_access.re <= 1'b1;
                program_access.raddr <= thread_context.program_counter;
                machine_state <= MACHINE_STATE_INSTRUCTION_RECEIVE;
            end

            MACHINE_STATE_CONTEXT_STORE:
            begin
                enableswitch <= 1'b0;
                thread_context_to_store.program_counter <= program_counter;
                thread_context_to_store.regs <= regs;
                thread_context_to_store.pc_context_store <= thread_context.pc_context_store;
                thread_context_to_store.pc_context_load <= thread_context.pc_context_load;

                program_counter <= thread_context.pc_context_store;
                program_access.re <= 1'b1;
                program_access.raddr <= thread_context.pc_context_store;
                machine_state <= MACHINE_STATE_INSTRUCTION_RECEIVE;
            end

            MACHINE_STATE_EXECUTE:
            begin
                if (program_counter == 8'hFF ) // Done
                begin
                    thread_context_to_store.program_counter <= 0;
                    thread_context_to_store.regs <= 0;
                    thread_context_to_store.pc_context_store <= thread_context.pc_context_store;
                    thread_context_to_store.pc_context_load <= thread_context.pc_context_load;
                    machine_state <= MACHINE_STATE_DONE;
                end
                else if (program_counter == 8'hF0) // Context Store Done
                begin
                    machine_state <= MACHINE_STATE_DONE;
                end
                else if (program_counter == 8'hF1) // Context Load Done
                begin
                    machine_state <= MACHINE_STATE_CONTEXT_LOAD_DONE;
                end
                else if (op_done[opcode[7:4]-1] || opcode[7:4] == 4'b0 || nonblocking)
                begin
                    regs[0] <= updateIndex(instruction[0], regs[0]);
                    regs[1] <= updateIndex(instruction[1], regs[1]);
                    regs[2] <= updateIndex(instruction[2], regs[2]);

                    if (enableswitch && context_switch)
                    begin
                        machine_state <= MACHINE_STATE_CONTEXT_STORE;
                    end
                    else
                    begin
                        program_access.re <= 1'b1;
                        program_access.raddr <= program_counter;
                        machine_state <= MACHINE_STATE_INSTRUCTION_RECEIVE;
                    end
                end
            end

            MACHINE_STATE_DONE:
            begin
                program_counter <= 0;
                machine_state <= MACHINE_STATE_IDLE;
            end
        endcase

        if (reset)
        begin
            program_counter <= 0;
            machine_state <= MACHINE_STATE_IDLE;
            opcode <= 8'b0;
            nonblocking <= 1'b0;
            enableswitch <= 1'b0;
        end
    end

    // *************************************************************************
    //
    //   Local Memories
    //
    // *************************************************************************

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_model_interface1();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_model_interface2();
    bram
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE))
    MEM_model1 (
        .clk,
        .access(MEM_model_interface1.bram_source)
    );
    bram
    #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE))
    MEM_model2 (
        .clk,
        .access(MEM_model_interface2.bram_source)
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

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) copy_MEM_model_interface();

    assign load_MEM_accessprops_interface.rvalid = MEM_accessprops_interface.rvalid;
    assign load_MEM_accessprops_interface.rdata = MEM_accessprops_interface.rdata;

    assign dot_MEM_model_interface.rvalid = MEM_model_interface1.rvalid;
    assign dot_MEM_model_interface.rdata = MEM_model_interface1.rdata;
    assign dot_MEM_labels_interface.rvalid = MEM_labels_interface.rvalid;
    assign dot_MEM_labels_interface.rdata = MEM_labels_interface.rdata;

    assign modify_MEM_labels_interface.rvalid = MEM_labels_interface.rvalid;
    assign modify_MEM_labels_interface.rdata = MEM_labels_interface.rdata;

    assign update_MEM_model_interface.rvalid = MEM_model_interface2.rvalid;
    assign update_MEM_model_interface.rdata = MEM_model_interface2.rdata;

    assign writeback_MEM_model_interface.rvalid = MEM_model_interface2.rvalid;
    assign writeback_MEM_model_interface.rdata = MEM_model_interface2.rdata;
    assign writeback_MEM_labels_interface.rvalid = MEM_labels_interface.rvalid;
    assign writeback_MEM_labels_interface.rdata = MEM_labels_interface.rdata;

    assign copy_MEM_model_interface.rvalid = MEM_model_interface2.rvalid;
    assign copy_MEM_model_interface.rdata = MEM_model_interface2.rdata;

    always_ff @(posedge clk)
    begin
        // MEM_model write arbitration
        MEM_model_interface1.we <= 1'b0;
        if (load_MEM_model_interface.we)
        begin
            MEM_model_interface1.we <= 1'b1;
            MEM_model_interface1.waddr <= load_MEM_model_interface.waddr;
            MEM_model_interface1.wdata <= load_MEM_model_interface.wdata;
        end
        else if (copy_MEM_model_interface.we)
        begin
            MEM_model_interface1.we <= 1'b1;
            MEM_model_interface1.waddr <= copy_MEM_model_interface.waddr;
            MEM_model_interface1.wdata <= copy_MEM_model_interface.wdata;
        end
    
        MEM_model_interface2.we <= 1'b0;
        if (load_MEM_model_interface.we)
        begin
            MEM_model_interface2.we <= 1'b1;
            MEM_model_interface2.waddr <= load_MEM_model_interface.waddr;
            MEM_model_interface2.wdata <= load_MEM_model_interface.wdata;
        end
        else if (update_MEM_model_interface.we)
        begin
            MEM_model_interface2.we <= 1'b1;
            MEM_model_interface2.waddr <= update_MEM_model_interface.waddr;
            MEM_model_interface2.wdata <= update_MEM_model_interface.wdata;
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

    always_comb
    begin
        // MEM_model request arbitration
        if (dot_MEM_model_interface.re)
        begin
            MEM_model_interface1.re = 1'b1;
            MEM_model_interface1.raddr = dot_MEM_model_interface.raddr;
        end
        else
        begin
            MEM_model_interface1.re = 1'b0;
            MEM_model_interface1.raddr = 0;
        end

        if (update_MEM_model_interface.re)
        begin
            MEM_model_interface2.re = 1'b1;
            MEM_model_interface2.raddr = update_MEM_model_interface.raddr;
        end
        else if (copy_MEM_model_interface.re)
        begin
            MEM_model_interface2.re = 1'b1;
            MEM_model_interface2.raddr = copy_MEM_model_interface.raddr;
        end
        else if (writeback_MEM_model_interface.re)
        begin
            MEM_model_interface2.re = 1'b1;
            MEM_model_interface2.raddr = writeback_MEM_model_interface.raddr;
        end
        else
        begin
            MEM_model_interface2.re = 1'b0;
            MEM_model_interface2.raddr = 0;
        end

        // MEM_labels request arbitration
        if (modify_MEM_labels_interface.re)
        begin
            MEM_labels_interface.re = 1'b1;
            MEM_labels_interface.raddr = modify_MEM_labels_interface.raddr;
        end
        else if (dot_MEM_labels_interface.re)
        begin
            MEM_labels_interface.re = 1'b1;
            MEM_labels_interface.raddr = dot_MEM_labels_interface.raddr;
        end
        else if (writeback_MEM_labels_interface.re)
        begin
            MEM_labels_interface.re = 1'b1;
            MEM_labels_interface.raddr = writeback_MEM_labels_interface.raddr;
        end
        else
        begin
            MEM_labels_interface.re = 1'b0;
            MEM_labels_interface.raddr = 0;
        end

        // MEM_accessprops request arbitration
        if (load_MEM_accessprops_interface.re)
        begin
            MEM_accessprops_interface.re = 1'b1;
            MEM_accessprops_interface.raddr = load_MEM_accessprops_interface.raddr;
        end
        else
        begin
            MEM_accessprops_interface.re = 1'b0;
            MEM_accessprops_interface.raddr = 0;
        end
    end

    // =========================================================================
    //
    //   DRAM Access
    //
    // =========================================================================

    glm_load
    execute_load
    (
        .clk,
        .reset,
        .op_start(op_start[1]),
        .op_done(op_done[1]),
        .in_trigger_dma(!op_active[0]),
        .regs(load_regs),
        .in_addr(thread_information.in_addr),
        .DMA_read(DMA_load_read.to_dma),
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
        .in_addr(thread_information.in_addr),
        .out_addr(thread_information.out_addr),
        .MEM_model(writeback_MEM_model_interface.bram_read),
        .MEM_labels(writeback_MEM_labels_interface.bram_read),
        .DMA_write(DMA_rm_write.to_dma)
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

    pipearch_copy
    MEM_model_copy
    (
        .clk,
        .reset,
        .op_start(op_start[6]),
        .op_done(op_done[6]),
        .regs(copy_regs),
        .MEM_read(copy_MEM_model_interface.bram_read),
        .MEM_write(copy_MEM_model_interface.bram_write)
    );

endmodule