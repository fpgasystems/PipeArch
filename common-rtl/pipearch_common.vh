`ifndef PIPEARCH_COMMON
`define PIPEARCH_COMMON

parameter LOG2_PREFETCH_SIZE = 9;
parameter PREFETCH_SIZE = 2**LOG2_PREFETCH_SIZE - 16;

parameter CLADDR_WIDTH = 42;
typedef logic [CLADDR_WIDTH-1:0] t_claddr;

parameter CLDATA_WIDTH  = 512;
typedef logic [CLDATA_WIDTH-1:0] t_cldata;

typedef struct packed {
    logic[15:0] offset;
    logic[15:0] length;
} bram_access_properties;

typedef struct packed {
    logic [63:0] data;
    logic en;
} config_registers;

interface dma_control ();
    logic idle;
    logic active;
    logic start;
    logic done;
    logic async;
    logic [31:0] regs [5];
    t_claddr addr;

    modport to_dma (
        input idle,
        input active,
        output start,
        input done,
        output async,
        output regs,
        output addr);

    modport at_dma (
        output idle,
        output active,
        input start,
        output done,
        input async,
        input regs,
        input addr);
endinterface

interface dma_interface
#(
    parameter DATA_WIDTH = 512,
    parameter ADDRESS_WIDTH = 42
)
();
    
    // Read Request
    logic                       tx_re;
    logic [ADDRESS_WIDTH-1:0]   tx_raddr;
    logic [1:0]                 tx_rlength; // 00 -> 1, 01 -> 2, 11 -> 4
    logic                       tx_ralmostfull;

    // Read Response
    logic                       rx_rvalid;
    logic [DATA_WIDTH-1:0]      rx_rdata;

    // Write Request
    logic                       tx_we;
    logic [DATA_WIDTH-1:0]      tx_wdata;
    logic                       tx_walmostfull;

    // Write Response
    logic                       rx_wvalid;

    modport to_dma_read (
        output tx_re,
        output tx_raddr,
        output tx_rlength,
        input tx_ralmostfull,

        input rx_rvalid,
        input rx_rdata);

    modport from_dma_read (
        input tx_re,
        input tx_raddr,
        input tx_rlength,
        output tx_ralmostfull,

        output rx_rvalid,
        output rx_rdata);

    modport to_dma_write (
        output tx_we,
        output tx_wdata,
        input tx_walmostfull,

        input rx_wvalid);

    modport from_dma_write (
        input tx_we,
        input tx_wdata,
        output tx_walmostfull,

        output rx_wvalid);
endinterface

interface internal_interface
#(
    parameter WIDTH = 32
)
();
    // Write
    logic                   we;
    logic [WIDTH-1:0]       wdata;

    // Read
    logic [WIDTH-1:0]       rdata;
    logic                   rvalid;

    // Status
    logic almostfull;

    modport from_commonread(
        output almostfull,
        input rvalid,
        input rdata);

    modport commonread_source(
        input almostfull,
        output rvalid,
        output rdata);

    modport to_commonwrite(
        input almostfull,
        output we,
        output wdata);

    modport commonwrite_source(
        output almostfull,
        input we,
        input wdata);

endinterface

interface fifobram_interface
#(
    parameter WIDTH = 32,
    parameter LOG2_DEPTH = 5
)
();
    // Write
    logic                   we;
    logic [LOG2_DEPTH-1:0]  waddr;
    logic [WIDTH-1:0]       wdata;

    // Request
    logic                   re;
    logic [LOG2_DEPTH-1:0]  raddr;

    // Read
    logic [WIDTH-1:0]       rdata;
    logic                   rvalid;

    // Status
    logic almostfull;
    logic empty;
    logic [LOG2_DEPTH-1:0] count;

    modport bram_write(
        output we,
        output waddr,
        output wdata);

    modport bram_read(
        output re,
        output raddr,
        input rdata,
        input rvalid);

    modport bram_readwrite(
        output we,
        output waddr,
        output wdata,
        output re,
        output raddr,
        input rdata,
        input rvalid);

    modport bram_source(
        input we,
        input waddr,
        input wdata,
        input re,
        input raddr,
        output rdata,
        output rvalid);

    modport fifo_write(
        output we,
        output wdata,
        input almostfull,
        input count);

    modport fifo_read(
        output re,
        input rdata,
        input rvalid,
        input empty);
    
    modport fifo_source(
        input we,
        input wdata,
        input re,
        output rdata,
        output rvalid,
        output almostfull,
        output empty,
        output count);
endinterface

// =================================
//
//   COMMON STATES
//
// =================================
typedef enum logic [2:0]
{
    RXTX_STATE_IDLE,
    RXTX_STATE_PROGRAM_READ,
    RXTX_STATE_CONTEXT_READ,
    RXTX_STATE_PROGRAM_EXECUTE,
    RXTX_STATE_DONE
} t_rxtxstate;

typedef enum logic [2:0]
{
    MACHINE_STATE_IDLE,
    MACHINE_STATE_INSTRUCTION_FETCH,
    MACHINE_STATE_INSTRUCTION_RECEIVE,
    MACHINE_STATE_INSTRUCTION_DECODE,
    MACHINE_STATE_EXECUTE,
    MACHINE_STATE_CONTEXT_LOAD_DONE,
    MACHINE_STATE_CONTEXT_STORE,
    MACHINE_STATE_DONE
} t_machinestate;

`endif // PIPEARCH_COMMON