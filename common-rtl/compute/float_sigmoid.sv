`include "pipearch_common.vh"

module float_sigmoid
(
    input logic clk,
    input logic reset,
    input logic [31:0] in,
    input logic in_valid,
    output logic [31:0] q,
    output logic q_valid
);

`ifdef XILINX
`else
    logic [31:0] one = 32'h3f800000;
    logic [31:0] minus_one = 32'hbf800000;
    logic [31:0] exp_input;
    logic [31:0] exp_output;
    logic [31:0] denom_output;
    assign exp_input = {!in[31], in[30:0]};

    fp_exp_arria10_nohard
    exp (
        .clk(clk),
        .areset(reset),
        .a(exp_input),
        .q(exp_output));

    fp_subtract_arria10
    sub (
        .clk(clk),
        .areset(reset),
        .a(exp_output),
        .b(minus_one),
        .q(denom_output));

    fp_div_arria10
    div (
        .clk(clk),
        .areset(reset),
        .a(one),
        .b(denom_output),
        .q(q));

    localparam LATENCY = 27;
    logic [LATENCY-1:0] status = 0;
    always_ff @(posedge clk)
    begin
        status[0] <= in_valid;
        for (int i = 1; i < LATENCY; i++)
        begin
            status[i] <= status[i-1];
        end
    end
    assign q_valid = status[LATENCY-1];
`endif

endmodule // float_sigmoid