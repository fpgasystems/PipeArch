`include "pipearch_common.vh"

module float_lessthan
(
    input logic clk,
    input logic reset,
    input logic [31:0] in1,
    input logic [31:0] in2,
    input logic in_valid,
    output logic q,
    output logic q_valid
);

`ifdef XILINX
    
`else
    fp_lt_arria10
    gt (
        .clk(clk),
        .areset(reset),
        .a(in1),
        .b(in2),
        .q(q));
    assign q_valid = in_valid;
    // localparam LATENCY = 3;
    // logic [LATENCY-1:0] status = 0;
    // always_ff @(posedge clk)
    // begin
    //     status[0] <= in_valid;
    //     for (int i = 1; i < LATENCY; i++)
    //     begin
    //         status[i] <= status[i-1];
    //     end
    // end
    // assign q_valid = status[LATENCY-1];
`endif

endmodule // float_lessthan