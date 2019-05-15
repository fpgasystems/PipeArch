`ifndef PIPEARCH_COMMON
`define PIPEARCH_COMMON

// `define XILINX

parameter LOG2_PREFETCH_SIZE = 9;
parameter PREFETCH_SIZE = 2**LOG2_PREFETCH_SIZE - 16;

parameter CLADDR_WIDTH = 42;
typedef logic [CLADDR_WIDTH-1:0] t_claddr;

parameter CLDATA_WIDTH  = 512;
typedef logic [CLDATA_WIDTH-1:0] t_cldata;

parameter CL_BYTE_IDX_BITS = 6; // 64 bytes

parameter LOG2_ACCESS_SIZE = 14;

typedef struct packed {
    logic write_fifo;
    logic write_bram;
    logic[LOG2_ACCESS_SIZE-1:0] length;
    logic keep_count_along_iterations;
    logic use_local_props;
    logic[LOG2_ACCESS_SIZE-1:0] offset;
} access_properties;

typedef struct packed {
    logic [63:0] data;
    logic en;
} config_registers;

typedef struct packed {
    logic [31:0] reg0;
    logic [31:0] reg1;
    logic [31:0] reg2;
    logic [31:0] reg3;
    logic [31:0] reg4;
} t_dma_reg;

typedef struct packed {
    logic share;
    logic consume;
    logic start;
    logic async;
    t_dma_reg regs;
    t_claddr addr;
} t_dma_control;

typedef struct packed {
    logic idle;
    logic active;
    logic done;
} t_dma_status;

typedef struct packed {
    // Read Request
    logic                       re;
    logic [CLADDR_WIDTH-1:0]    raddr;
    logic [1:0]                 rlength; // 00 -> 1, 01 -> 2, 11 -> 4
} t_dma_tx_read;

typedef struct packed {
    // Read Response
    logic                       rvalid;
    logic [CLDATA_WIDTH-1:0]    rdata;
    logic                       ralmostfull;
} t_dma_rx_read;

typedef struct packed {
    // Write Request
    logic                       we;
    logic [CLDATA_WIDTH-1:0]    wdata;
} t_dma_tx_write;

typedef struct packed {
    // Write Response
    logic                       wvalid;
    logic                       walmostfull;
} t_dma_rx_write;

interface dma_read_interface();
    t_dma_control control;
    t_dma_status status;
    t_dma_tx_read tx_read;
    t_dma_rx_read rx_read;

    modport to_dma (
        output control,
        input status,
        output tx_read,
        input rx_read);

    modport at_dma (
        input control,
        output status,
        input tx_read,
        output rx_read);
endinterface

interface dma_write_interface();
    t_dma_control control;
    t_dma_status status;
    t_dma_tx_write tx_write;
    t_dma_rx_write rx_write;

    modport to_dma (
        output control,
        input status,
        output tx_write,
        input rx_write);

    modport at_dma (
        input control,
        output status,
        input tx_write,
        output rx_write);
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
    // wfifobram 01 -> bram
    // wfifobram 10 -> fifo
    // wfifobram 11 -> both
    logic [1:0]             wfifobram;

    // Request
    logic                   re;
    logic [LOG2_DEPTH-1:0]  raddr;
    // rfifobram 01 -> bram
    // rfifobram 10 -> fifo
    logic [1:0]             rfifobram;

    // Read
    logic [WIDTH-1:0]       rdata;
    logic                   rvalid;

    // Status
    logic almostfull;
    logic empty;
    logic [LOG2_DEPTH-1:0] count;

    modport write(
        output we,
        output waddr,
        output wdata,
        output wfifobram,
        input almostfull,
        input count);

    modport writeforward(
        input we,
        input waddr,
        input wdata,
        input wfifobram);

    modport read(
        output re,
        output raddr,
        output rfifobram,
        input rdata,
        input rvalid,
        input empty);

    modport source(
        input we,
        input waddr,
        input wdata,
        input wfifobram,
        output almostfull,
        output count,
        input re,
        input raddr,
        input rfifobram,
        output rdata,
        output rvalid,
        output empty);

    modport write_source(
        input we,
        input waddr,
        input wdata,
        input wfifobram,
        output almostfull,
        output count);

    modport read_source(
        input re,
        input raddr,
        input rfifobram,
        output rdata,
        output rvalid,
        output empty);

    modport bram_source(
        input we,
        input waddr,
        input wdata,
        input re,
        input raddr,
        output rdata,
        output rvalid);

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
    RXTX_STATE_CONTEXT_WRITE,
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