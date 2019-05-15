`ifndef GLM_COMMON
`define GLM_COMMON

parameter LOG2_PREFETCH_BUFFER_SIZE = 10;
parameter LOG2_MEMORY_SIZE = 10;
parameter LOG2_PROGRAM_SIZE = 8;
parameter PROGRAM_SIZE = 2**LOG2_PROGRAM_SIZE;
parameter NUM_REGS = 5;
parameter REGS_WIDTH = 32*NUM_REGS;
parameter LOG2_INTERNAL_SIZE = 6;
parameter NUM_OPS = 14;

`endif // GLM_COMMON