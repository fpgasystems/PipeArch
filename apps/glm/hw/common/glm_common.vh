`ifndef GLM_COMMON
`define GLM_COMMON

parameter LOG2_MEMORY_SIZE = 10;
parameter LOG2_PROGRAM_SIZE = 8;
parameter PROGRAM_SIZE = 2**LOG2_PROGRAM_SIZE;
parameter NUM_REGS = 3;
parameter REGS_WIDTH = 32*NUM_REGS;
parameter LOG2_INTERNAL_SIZE = 6;
parameter NUM_OPS = 10;

// *************************************************************************
//
//   NUM_LOAD_CHANNELS
//
// *************************************************************************
parameter NUM_LOAD_CHANNELS = 5;

// *************************************************************************
//
//   NUM_WRITEBACK_CHANNELS
//
// *************************************************************************
parameter NUM_WRITEBACK_CHANNELS = 2;

`endif // GLM_COMMON