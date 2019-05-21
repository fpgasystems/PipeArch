`include "pipearch_common.vh"

module float_greaterthan
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
    logic [7:0] temp_q;
    xlnx_fp_gt
    mult (
        .s_axis_a_tvalid(in_valid),
        .s_axis_a_tdata(in1),
        .s_axis_b_tvalid(in_valid),
        .s_axis_b_tdata(in2),
        .m_axis_result_tvalid(q_valid),
        .m_axis_result_tdata(temp_q));
    assign q = temp_q[0];
`else
    fp_gt_arria10
    gt (
        .clk(clk),
        .areset(reset),
        .a(in1),
        .b(in2),
        .q(q));
    assign q_valid = in_valid;
`endif

endmodule // float_greaterthan