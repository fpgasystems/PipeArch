`ifndef PIPEARCH_COMMON
`define PIPEARCH_COMMON

parameter LOG2_PREFETCH_SIZE = 9;
parameter PREFETCH_SIZE = 2**LOG2_PREFETCH_SIZE - 16;

typedef struct packed {
    logic[15:0] offset;
    logic[15:0] length;
} bram_access_properties;

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

typedef enum logic [1:0]
{
    RXTX_STATE_IDLE,
    RXTX_STATE_PROGRAM_READ,
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
    MACHINE_STATE_DONE
} t_machinestate;

`endif // PIPEARCH_COMMON