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
    typedef logic [$bits(t_claddr) + CL_BYTE_IDX_BITS - 1 : 0] t_byteaddr;
    function automatic t_claddr byteAddrToClAddr(t_byteaddr addr);
// `ifdef XILINX
//         return addr[0 +: $bits(t_claddr)];
// `else
        return addr[CL_BYTE_IDX_BITS +: $bits(t_claddr)];
// `endif
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
    // just block on opcode = instruction[15][10]

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
    // reg[0] = reg[0]*instruction[10]                  // instruction[10]: read offset change per index0
    // reg[1] = reg[1]*instruction[11]                  // instruction[11]: read offset change per index1
    // reg[2] = reg[2]*instruction[12]                  // instruction[12]: read offset change per index2
    // reg[3] = instruction[3]                          // DRAM read offset in cachelines
    // reg[4] = instruction[4]                          // DRAM read length in cachelines

    // if opcode == 0x2N ---- load
    // reg[0] = reg[0]*instruction[10]                  // instruction[10]: read offset change per index0
    // reg[1] = reg[1]*instruction[11]                  // instruction[11]: read offset change per index1
    // reg[2] = reg[2]*instruction[12]                  // instruction[12]: read offset change per index2
    // reg[3] = instruction[3]                          // DRAM read offset in cachelines
    // reg[4] = instruction[4]                          // DRAM read length in cachelines
    // reg[5] = instruction[5]                          // output access properties
    // reg[6] = instruction[6]                          // output channel selection

    // if opcode == 0x3N ---- writeback
    // reg[0] = reg[0]*instruction[10]                  // instruction[10]: read offset change per index0
    // reg[1] = reg[1]*instruction[11]                  // instruction[11]: read offset change per index1
    // reg[2] = reg[2]*instruction[12]                  // instruction[12]: read offset change per index2
    // reg[3] = instruction[3]                          // [30:0] DRAM store offset in cachelines
                                                        // [31] DRAM buffer (0 out) (1 in)
    // reg[4] = instruction[4]                          // DRAM store length in cachelines
    // reg[5] = instruction[5]                          // [3:0] internal read channel select
                                                        // [4] write Fence
    // reg[6] = instruction[6]                          // input access properties

    // if opcode == 0x4N ---- dot
    // reg[0] = instruction[3]                          // [15:0] num lines to process
                                                        // [31:16] num iterations
    // reg[1] = instruction[4]                          // left input access properties
    // reg[2] = instruction[5]                          // right input access properties
    // reg[3] = instruction[6]                          // output access properties
    // reg[4] = instruction[7]                          // [0] do sigmoid on dot

    // if opcode == 0x5N ---- modify
    // reg[0] = reg[0]                                  // index 0
    // reg[1] = instruction[3]                          // [31:16] num iterations
    // reg[2] = instruction[4]                          // labels input access properties
    // reg[3] = instruction[5]                          // [1:0]: (0 linreg) (1 logreg) (2 SVM)
                                                        // [2]: (0 SGD) (1 SCD)
                                                        // [3]: Use index 0
    // reg[4] = instruction[6]                          // step size
    // reg[5] = instruction[7]                          // lambda
    // reg[6] = instruction[8]                          // gradient output access properties

    // if opcode == 0x6N ---- update
    // reg[0] = instruction[3]                          // [15:0] num lines to process
                                                        // [31:16] num iterations
    // reg[1] = instruction[4]                          // samples access properties
    // reg[2] = instruction[5]                          // gradient access properties
    // reg[3] = instruction[6]                          // model read access properties
    // reg[4] = instruction[7]                          // model write access properties

    // if opcode == 0x7N ---- copy
    // reg[0] = instruction[3]                          // source access properties
    // reg[1] = instruction[4]                          // destination access properties
    // reg[2] = instruction[5]                          // destination channel select

    // if opcode == 0x8N ---- synchronize

    // if opcode == 0x9N ---- delta
    // reg[0] = instruction[3]                          // [15:0] num lines to process
                                                        // [31:16] num iterations
    // reg[1] = instruction[4]                          // left input access properties
    // reg[2] = instruction[5]                          // right input access properties
    // reg[3] = instruction[6]                          // output access properties
    // reg[4] = instruction[7]                          // [0] do sigmoid on model input

    // if opcode == 0xaN ---- update
    // reg[0] = instruction[3]                          // [15:0] num lines to process
                                                        // [31:16] num iterations
    // reg[1] = instruction[4]                          // model access properties
    // reg[2] = instruction[5]                          // gradient access properties
    // reg[3] = instruction[6]                          // samples read access properties
    // reg[4] = instruction[7]                          // samples write access properties
    // reg[5] = instruction[8]                          // [0] enable async

    // if opcode == 0xbN ---- load to reg
    // loadreg_regs[0] = reg[0]                         // index 0
    // loadreg_regs[1] = instruction[3]                 // which register to load the 32-bit value (actual range [3,NUM_REGS-1], because [0,2] are reserved)
    // loadreg_regs[2] = instruction[4]                 // BRAM load offset in cachelines
    // result from on-chip memory -> loadreg_outregs[x]

    // if opcode == 0xcN ---- l2reg
    // reg[0] = instruction[3]                          // [15:0] num lines to process
    // reg[1] = instruction[4]                          // modelold read access properties
    // reg[2] = instruction[5]                          // modelnew read access properties
    // reg[3] = instruction[6]                          // modelforward write access properties
    // reg[4] = instruction[7]                          // modelnew write access properties
    // reg[5] = instruction[8]                          // lambda

    // if opcode = 0xdN ---- writeforward1
    // reg[0] = instruction[3]                          // [15:0] num lines to process
                                                        // [31:16] num iterations

    // if opcode = 0xeN ---- writeforward2
    // reg[0] = instruction[3]                          // [15:0] num lines to process
                                                        // [31:16] num iterations

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
    logic [31:0] load_regs [7];
    logic [31:0] writeback_regs [7];
    logic [31:0] dot_regs [5];
    logic [31:0] delta_regs [5];
    logic [31:0] modify_regs [7];
    logic [31:0] update1_regs [6];
    logic [31:0] update2_regs [6];
    logic [31:0] copy_regs [3];
    logic [31:0] loadreg_regs[3];
    logic [31:0] loadreg_outregs [NUM_REGS];
    logic [31:0] l2reg_regs[6];
    logic [31:0] forward1_regs[1];
    logic [31:0] forward2_regs[1];

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
                            op_start[0] <= !instruction[15][10];
                            prefetch_regs.reg0 <= DSP27Mult(temp_regs[0],instruction[10]);
                            prefetch_regs.reg1 <= DSP27Mult(temp_regs[1],instruction[11]);
                            prefetch_regs.reg2 <= DSP27Mult(temp_regs[2],instruction[12]);
                            prefetch_regs.reg3 <= instruction[3]; // read offset
                            prefetch_regs.reg4 <= instruction[4]; // read length in cachelines
                        end

                        4'h2: // load
                        begin
                            op_start[1] <= !instruction[15][10];
                            load_regs[0] <= DSP27Mult(temp_regs[0],instruction[10]);
                            load_regs[1] <= DSP27Mult(temp_regs[1],instruction[11]);
                            load_regs[2] <= DSP27Mult(temp_regs[2],instruction[12]);
                            load_regs[3] <= instruction[3]; // read offset
                            load_regs[4] <= instruction[4]; // read length in cachelines
                            load_regs[5] <= instruction[5];
                            load_regs[6] <= instruction[6];
                        end

                        4'h3: // writeback
                        begin
                            op_start[2] <= !instruction[15][10];
                            writeback_regs[0] <= DSP27Mult(temp_regs[0],instruction[10]);
                            writeback_regs[1] <= DSP27Mult(temp_regs[1],instruction[11]);
                            writeback_regs[2] <= DSP27Mult(temp_regs[2],instruction[12]);
                            writeback_regs[3] <= instruction[3]; // store offset
                            writeback_regs[4] <= instruction[4]; // store length in cachelines
                            writeback_regs[5] <= instruction[5]; // channel select
                            writeback_regs[6] <= instruction[6];
                        end

                        // *************************************************************************
                        //
                        //   Additional opcodes
                        //
                        // *************************************************************************
                        4'h4: // dot
                        begin
                            op_start[3] <= !instruction[15][10];
                            dot_regs[0] <= (instruction[3][31:16] == 16'hFFFF) ? temp_regs[3] : instruction[3];
                            dot_regs[1] <= instruction[4];
                            dot_regs[2] <= instruction[5];
                            dot_regs[3] <= instruction[6];
                            dot_regs[4] <= instruction[7];
                        end

                        4'h5: // modify
                        begin
                            op_start[4] <= !instruction[15][10];
                            modify_regs[0] <= temp_regs[0];
                            modify_regs[1] <= (instruction[3][31:16] == 16'hFFFF) ? temp_regs[3] : instruction[3];
                            modify_regs[2] <= instruction[4];
                            modify_regs[3] <= instruction[5];
                            modify_regs[4] <= instruction[6];
                            modify_regs[5] <= instruction[7];
                            modify_regs[6] <= instruction[8];
                        end

                        4'h6: // update1
                        begin
                            op_start[5] <= !instruction[15][10];
                            update1_regs[0] <= (instruction[3][31:16] == 16'hFFFF) ? temp_regs[3] : instruction[3];
                            update1_regs[1] <= instruction[4];
                            update1_regs[2] <= instruction[5];
                            update1_regs[3] <= instruction[6];
                            update1_regs[4] <= instruction[7];
                            update1_regs[5] <= instruction[8];
                        end

                        4'ha: // update2
                        begin
                            op_start[9] <= !instruction[15][10];
                            update2_regs[0] <= (instruction[3][31:16] == 16'hFFFF) ? temp_regs[3] : instruction[3];
                            update2_regs[1] <= instruction[4];
                            update2_regs[2] <= instruction[5];
                            update2_regs[3] <= instruction[6];
                            update2_regs[4] <= instruction[7];
                            update2_regs[5] <= instruction[8];
                        end

                        4'h7: // copy
                        begin
                            op_start[6] <= !instruction[15][10];
                            copy_regs[0] <= instruction[3];
                            copy_regs[1] <= instruction[4];
                            copy_regs[2] <= instruction[5];
                        end

                        4'h8: // synchronize
                        begin
                            op_start[7] <= !instruction[15][10];
                        end

                        4'h9: // delta
                        begin
                            op_start[8] <= !instruction[15][10];
                            delta_regs[0] <= instruction[3];
                            delta_regs[1] <= instruction[4];
                            delta_regs[2] <= instruction[5];
                            delta_regs[3] <= instruction[6];
                            delta_regs[4] <= instruction[7];
                        end

                        4'hb: // loadreg
                        begin
                            op_start[10] <= !instruction[15][10];
                            loadreg_regs[0] <= temp_regs[0];
                            loadreg_regs[1] <= instruction[3];
                            loadreg_regs[2] <= instruction[4];
                        end

                        4'hc:
                        begin
                            op_start[11] <= !instruction[15][10];
                            l2reg_regs[0] <= instruction[3];
                            l2reg_regs[1] <= instruction[4];
                            l2reg_regs[2] <= instruction[5];
                            l2reg_regs[3] <= instruction[6];
                            l2reg_regs[4] <= instruction[7];
                            l2reg_regs[5] <= instruction[8];
                        end

                        4'hd:
                        begin
                            op_start[12] <= !instruction[15][10];
                            forward1_regs[0] <= (instruction[3][31:16] == 16'hFFFF) ? temp_regs[3] : instruction[3];
                        end

                        4'he:
                        begin
                            op_start[13] <= !instruction[15][10];
                            forward2_regs[0] <= (instruction[3][31:16] == 16'hFFFF) ? temp_regs[3] : instruction[3];
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

                        4'h4:
                        begin
                            program_counter <= conditional(temp_regs[3], instruction[13], instruction[14][31:16], instruction[14][15:0]);
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
                    regs[3] <= loadreg_outregs[3];
                    regs[4] <= loadreg_outregs[4];

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
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_BUFFER_SIZE)) REGION_prefetch_write[1]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PREFETCH_BUFFER_SIZE)) REGION_prefetch_read[1]();
    region_exclusive
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_PREFETCH_BUFFER_SIZE), .NUM_WRITE_CHANNELS(1), .NUM_READ_CHANNELS(1))
    REGION_prefetch (
        .clk, .reset,
        .write_access(REGION_prefetch_write),
        .read_access(REGION_prefetch_read)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_input_write[3]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_input_read[3]();
    region_exclusive_replicate
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_WRITE_CHANNELS(3), .NUM_READ_CHANNELS(3))
    REGION_input (
        .clk, .reset,
        .write_access(REGION_input_write),
        .read_access(REGION_input_read)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_inputcopy_write[2]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_inputcopy_read[2]();
    region_exclusive
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_WRITE_CHANNELS(2), .NUM_READ_CHANNELS(2))
    REGION_inputcopy (
        .clk, .reset,
        .write_access(REGION_inputcopy_write),
        .read_access(REGION_inputcopy_read)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_model1_write[3]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_model1_read[3]();
    region_exclusive_replicate
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_WRITE_CHANNELS(3), .NUM_READ_CHANNELS(3))
    REGION_model1 (
        .clk, .reset,
        .write_access(REGION_model1_write),
        .read_access(REGION_model1_read)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_model2_interface[4]();
    region
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_CHANNELS(4))
    REGION_model2 (
        .clk, .reset,
        .access(REGION_model2_interface)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_labels_write[3]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) REGION_labels_read[3]();
    region_exclusive
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_WRITE_CHANNELS(3), .NUM_READ_CHANNELS(3))
    REGION_labels (
        .clk, .reset,
        .write_access(REGION_labels_write),
        .read_access(REGION_labels_read)
    );

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) REGION_dot_write[1]();
    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) REGION_dot_read[1]();
    fifo_replicate
    #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE), .NUM_WRITE_CHANNELS(1), .NUM_READ_CHANNELS(1))
    REGION_dot (
        .clk, .reset,
        .write_access(REGION_dot_write),
        .read_access(REGION_dot_read)
    );

    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) REGION_gradient_write[1]();
    fifobram_interface #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE)) REGION_gradient_read[2]();
    fifo_replicate
    #(.WIDTH(32), .LOG2_DEPTH(LOG2_INTERNAL_SIZE), .NUM_WRITE_CHANNELS(1), .NUM_READ_CHANNELS(2))
    REGION_gradient (
        .clk, .reset,
        .write_access(REGION_gradient_write),
        .read_access(REGION_gradient_read)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_accessprops_write[1]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_accessprops_read[1]();
    bram_replicate
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_WRITE_CHANNELS(1), .NUM_READ_CHANNELS(1))
    MEM_accessprops (
        .clk, .reset,
        .write_access(MEM_accessprops_write),
        .read_access(MEM_accessprops_read)
    );

    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_localprops_write[1]();
    fifobram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_MEMORY_SIZE)) MEM_localprops_read[7]();
    bram_replicate
    #(.WIDTH(CLDATA_WIDTH), .LOG2_DEPTH(LOG2_MEMORY_SIZE), .NUM_WRITE_CHANNELS(1), .NUM_READ_CHANNELS(7))
    MEM_localprops (
        .clk, .reset,
        .write_access(MEM_localprops_write),
        .read_access(MEM_localprops_read)
    );

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
        .REGION0_write(REGION_input_write[0].write),
        .REGION1_write(REGION_model2_interface[0].write),
        .REGION2_write(REGION_labels_write[0].write),
        .REGION_prefetch_write(REGION_prefetch_write[0].write),
        .MEM_localprops_write(MEM_localprops_write[0].write),
        .MEM_accessprops_write(MEM_accessprops_write[0].write),
        .MEM_accessprops_read(MEM_accessprops_read[0].read)
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
        .REGION0_read(REGION_model2_interface[2].read),
        .REGION1_read(REGION_labels_read[2].read),
        .REGION2_read(REGION_inputcopy_read[1].read),
        .DMA_write(DMA_rm_write.to_dma)
    );

    // *************************************************************************
    //
    //   Local Computation
    //
    // *************************************************************************
    glm_delta
    execute_delta
    (
        .clk,
        .reset,
        .op_start(op_start[8]),
        .op_done(op_done[8]),
        .regs(delta_regs),
        .REGION_left_read(REGION_model2_interface[3].read),
        .REGION_right_read(REGION_labels_read[1].read),
        .REGION_delta_write(REGION_model1_write[1].write)
    );

    glm_dot
    execute_dot
    (
        .clk,
        .reset,
        .op_start(op_start[3]),
        .op_done(op_done[3]),
        .regs(dot_regs),
        .MEM_props_left(MEM_localprops_read[0].read),
        .MEM_props_right(MEM_localprops_read[1].read),
        .REGION_left_read(REGION_input_read[0].read),
        .REGION_right_read(REGION_model1_read[0].read),
        .REGION_dot_write(REGION_dot_write[0].write)
    );

    glm_modify
    execute_modify
    (
        .clk,
        .reset,
        .op_start(op_start[4]),
        .op_done(op_done[4]),
        .regs(modify_regs),
        .MEM_labels_read(REGION_labels_read[0].read),
        .FIFO_dot_read(REGION_dot_read[0].read),
        .MEM_labels_write(REGION_labels_write[1].write),
        .REGION_gradient_write(REGION_gradient_write[0].write)
    );

    glm_update
    execute_update1
    (
        .clk,
        .reset,
        .op_start(op_start[5]),
        .op_done(op_done[5]),
        .regs(update1_regs),
        .MEM_props_samples(MEM_localprops_read[2]),
        .MEM_props_model(MEM_localprops_read[3]),
        .REGION_samples_read(REGION_input_read[1].read),
        .REGION_gradient_read(REGION_gradient_read[0].read),
        .MEM_model_read(REGION_model2_interface[0].read),
        .REGION_model_write(REGION_model2_interface[1].write)
    );

    glm_update
    execute_update2
    (
        .clk,
        .reset,
        .op_start(op_start[9]),
        .op_done(op_done[9]),
        .regs(update2_regs),
        .MEM_props_samples(MEM_localprops_read[4]),
        .MEM_props_model(MEM_localprops_read[5]),
        .REGION_samples_read(REGION_model1_read[1].read),
        .REGION_gradient_read(REGION_gradient_read[1].read),
        .MEM_model_read(REGION_inputcopy_read[0].read),
        .REGION_model_write(REGION_inputcopy_write[1].write)
    );

    glm_l2reg
    execute_l2reg
    (
        .clk,
        .reset,
        .op_start(op_start[11]),
        .op_done(op_done[11]),
        .regs(l2reg_regs),
        .REGION_modelold_read(REGION_model1_read[2].read),
        .REGION_modelnew_read(REGION_model2_interface[1].read),
        .REGION_modelforward_write(REGION_model1_write[0].write),
        .REGION_modelnew_write(REGION_model2_interface[2].write)
    );

    pipearch_copy
    MEM_model_copy
    (
        .clk,
        .reset,
        .op_start(op_start[6]),
        .op_done(op_done[6]),
        .regs(copy_regs),
        .REGION_read(REGION_prefetch_read[0].read),
        .REGION0_write(REGION_input_write[2].write),
        .REGION1_write(REGION_inputcopy_write[0].write),
        .REGION2_write(REGION_labels_write[2].write)
    );

    pipearch_writeforward
    MEM_model_forward
    (
        .clk,
        .reset,
        .op_start(op_start[12]),
        .op_done(op_done[12]),
        .regs(forward1_regs),
        .REGION_writeforward(REGION_model2_interface[1].writeforward),
        .REGION_write(REGION_model1_write[2].write)
    );

    pipearch_writeforward
    MEM_input_forward
    (
        .clk,
        .reset,
        .op_start(op_start[13]),
        .op_done(op_done[13]),
        .regs(forward2_regs),
        .REGION_writeforward(REGION_inputcopy_write[1].writeforward),
        .REGION_write(REGION_input_write[1].write)
    );

    pipearch_loadreg
    execute_loadreg
    (
        .clk,
        .reset,
        .op_start(op_start[10]),
        .op_done(op_done[10]),
        .regs(loadreg_regs),
        .outregs(loadreg_outregs),
        .REGION_read(MEM_localprops_read[6])
    );

endmodule
