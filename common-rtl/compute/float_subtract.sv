`include "pipearch_common.vh"

module float_subtract
(
    input logic clk,
    input logic reset,
    input logic [31:0] in1,
    input logic [31:0] in2,
    input logic in_valid,
    output logic [31:0] q,
    output logic q_valid
);

`ifdef XILINX
    xlnx_fp_subtract
    subtract (
        .aclk(clk),
        .s_axis_a_tvalid(in_valid),
        .s_axis_a_tdata(in1),
        .s_axis_b_tvalid(in_valid),
        .s_axis_b_tdata(in2),
        .m_axis_result_tvalid(q_valid),
        .m_axis_result_tdata(q));
`else
    fp_subtract_arria10
    subtract (
        .clk(clk),
        .areset(reset),
        .a(in1),
        .b(in2),
        .q(q));
    localparam LATENCY = 3;
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

endmodule // float_subtract