`include "pipearch_common.vh"

module dec23_to_float_convert
(
    input logic clk,
    input logic reset,
    input logic [47:0] in1,
    input logic in1_valid,
    output logic [31:0] q,
    output logic q_valid
);

`ifdef XILINX
    xlnx_dec23_to_fp_convert
    convert (
        .aclk(clk),
        .s_axis_a_tvalid(in1_valid),
        .s_axis_a_tdata(in1),
        .m_axis_result_tvalid(q_valid),
        .m_axis_result_tdata(q));
`else
    fp_converter48_arria10
    convert (
        .clk(clk),
        .areset(reset),
        .a(in1),
        .q(q));
    localparam LATENCY = 4;
    logic [LATENCY-1:0] status = 0;
    always_ff @(posedge clk)
    begin
        status[0] <= in1_valid;
        for (int i = 1; i < LATENCY; i++)
        begin
            status[i] <= status[i-1];
        end
    end
    assign q_valid = status[LATENCY-1];
`endif

endmodule // dec23_to_float_convert