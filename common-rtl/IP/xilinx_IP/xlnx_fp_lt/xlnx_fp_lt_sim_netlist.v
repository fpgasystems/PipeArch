// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2018.2 (lin64) Build 2258646 Thu Jun 14 20:02:38 MDT 2018
// Date        : Mon May 20 13:23:54 2019
// Host        : kkara-desktop running 64-bit Ubuntu 16.04.5 LTS
// Command     : write_verilog -force -mode funcsim
//               /home/kkara/Projects/pipearch/apps/glm/xilinx_work/ip_project/ip_project.srcs/sources_1/ip/xlnx_fp_lt/xlnx_fp_lt_sim_netlist.v
// Design      : xlnx_fp_lt
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xcvu9p-fsgd2104-2L-e
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CHECK_LICENSE_TYPE = "xlnx_fp_lt,floating_point_v7_1_6,{}" *) (* downgradeipidentifiedwarnings = "yes" *) (* x_core_info = "floating_point_v7_1_6,Vivado 2018.2" *) 
(* NotValidForBitStream *)
module xlnx_fp_lt
   (s_axis_a_tvalid,
    s_axis_a_tdata,
    s_axis_b_tvalid,
    s_axis_b_tdata,
    m_axis_result_tvalid,
    m_axis_result_tdata);
  (* x_interface_info = "xilinx.com:interface:axis:1.0 S_AXIS_A TVALID" *) (* x_interface_parameter = "XIL_INTERFACENAME S_AXIS_A, TDATA_NUM_BYTES 4, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 0, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.000, LAYERED_METADATA undef" *) input s_axis_a_tvalid;
  (* x_interface_info = "xilinx.com:interface:axis:1.0 S_AXIS_A TDATA" *) input [31:0]s_axis_a_tdata;
  (* x_interface_info = "xilinx.com:interface:axis:1.0 S_AXIS_B TVALID" *) (* x_interface_parameter = "XIL_INTERFACENAME S_AXIS_B, TDATA_NUM_BYTES 4, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 0, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.000, LAYERED_METADATA undef" *) input s_axis_b_tvalid;
  (* x_interface_info = "xilinx.com:interface:axis:1.0 S_AXIS_B TDATA" *) input [31:0]s_axis_b_tdata;
  (* x_interface_info = "xilinx.com:interface:axis:1.0 M_AXIS_RESULT TVALID" *) (* x_interface_parameter = "XIL_INTERFACENAME M_AXIS_RESULT, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 0, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.000, LAYERED_METADATA undef" *) output m_axis_result_tvalid;
  (* x_interface_info = "xilinx.com:interface:axis:1.0 M_AXIS_RESULT TDATA" *) output [7:0]m_axis_result_tdata;

  wire \<const0> ;
  wire [0:0]\^m_axis_result_tdata ;
  wire m_axis_result_tvalid;
  wire [31:0]s_axis_a_tdata;
  wire s_axis_a_tvalid;
  wire [31:0]s_axis_b_tdata;
  wire s_axis_b_tvalid;
  wire NLW_U0_m_axis_result_tlast_UNCONNECTED;
  wire NLW_U0_s_axis_a_tready_UNCONNECTED;
  wire NLW_U0_s_axis_b_tready_UNCONNECTED;
  wire NLW_U0_s_axis_c_tready_UNCONNECTED;
  wire NLW_U0_s_axis_operation_tready_UNCONNECTED;
  wire [7:1]NLW_U0_m_axis_result_tdata_UNCONNECTED;
  wire [0:0]NLW_U0_m_axis_result_tuser_UNCONNECTED;

  assign m_axis_result_tdata[7] = \<const0> ;
  assign m_axis_result_tdata[6] = \<const0> ;
  assign m_axis_result_tdata[5] = \<const0> ;
  assign m_axis_result_tdata[4] = \<const0> ;
  assign m_axis_result_tdata[3] = \<const0> ;
  assign m_axis_result_tdata[2] = \<const0> ;
  assign m_axis_result_tdata[1] = \<const0> ;
  assign m_axis_result_tdata[0] = \^m_axis_result_tdata [0];
  GND GND
       (.G(\<const0> ));
  (* C_ACCUM_INPUT_MSB = "32" *) 
  (* C_ACCUM_LSB = "-31" *) 
  (* C_ACCUM_MSB = "32" *) 
  (* C_A_FRACTION_WIDTH = "24" *) 
  (* C_A_TDATA_WIDTH = "32" *) 
  (* C_A_TUSER_WIDTH = "1" *) 
  (* C_A_WIDTH = "32" *) 
  (* C_BRAM_USAGE = "0" *) 
  (* C_B_FRACTION_WIDTH = "24" *) 
  (* C_B_TDATA_WIDTH = "32" *) 
  (* C_B_TUSER_WIDTH = "1" *) 
  (* C_B_WIDTH = "32" *) 
  (* C_COMPARE_OPERATION = "1" *) 
  (* C_C_FRACTION_WIDTH = "24" *) 
  (* C_C_TDATA_WIDTH = "32" *) 
  (* C_C_TUSER_WIDTH = "1" *) 
  (* C_C_WIDTH = "32" *) 
  (* C_FIXED_DATA_UNSIGNED = "0" *) 
  (* C_HAS_ABSOLUTE = "0" *) 
  (* C_HAS_ACCUMULATOR_A = "0" *) 
  (* C_HAS_ACCUMULATOR_S = "0" *) 
  (* C_HAS_ACCUM_INPUT_OVERFLOW = "0" *) 
  (* C_HAS_ACCUM_OVERFLOW = "0" *) 
  (* C_HAS_ACLKEN = "0" *) 
  (* C_HAS_ADD = "0" *) 
  (* C_HAS_ARESETN = "0" *) 
  (* C_HAS_A_TLAST = "0" *) 
  (* C_HAS_A_TUSER = "0" *) 
  (* C_HAS_B = "1" *) 
  (* C_HAS_B_TLAST = "0" *) 
  (* C_HAS_B_TUSER = "0" *) 
  (* C_HAS_C = "0" *) 
  (* C_HAS_COMPARE = "1" *) 
  (* C_HAS_C_TLAST = "0" *) 
  (* C_HAS_C_TUSER = "0" *) 
  (* C_HAS_DIVIDE = "0" *) 
  (* C_HAS_DIVIDE_BY_ZERO = "0" *) 
  (* C_HAS_EXPONENTIAL = "0" *) 
  (* C_HAS_FIX_TO_FLT = "0" *) 
  (* C_HAS_FLT_TO_FIX = "0" *) 
  (* C_HAS_FLT_TO_FLT = "0" *) 
  (* C_HAS_FMA = "0" *) 
  (* C_HAS_FMS = "0" *) 
  (* C_HAS_INVALID_OP = "0" *) 
  (* C_HAS_LOGARITHM = "0" *) 
  (* C_HAS_MULTIPLY = "0" *) 
  (* C_HAS_OPERATION = "0" *) 
  (* C_HAS_OPERATION_TLAST = "0" *) 
  (* C_HAS_OPERATION_TUSER = "0" *) 
  (* C_HAS_OVERFLOW = "0" *) 
  (* C_HAS_RECIP = "0" *) 
  (* C_HAS_RECIP_SQRT = "0" *) 
  (* C_HAS_RESULT_TLAST = "0" *) 
  (* C_HAS_RESULT_TUSER = "0" *) 
  (* C_HAS_SQRT = "0" *) 
  (* C_HAS_SUBTRACT = "0" *) 
  (* C_HAS_UNDERFLOW = "0" *) 
  (* C_LATENCY = "0" *) 
  (* C_MULT_USAGE = "0" *) 
  (* C_OPERATION_TDATA_WIDTH = "8" *) 
  (* C_OPERATION_TUSER_WIDTH = "1" *) 
  (* C_OPTIMIZATION = "1" *) 
  (* C_RATE = "1" *) 
  (* C_RESULT_FRACTION_WIDTH = "0" *) 
  (* C_RESULT_TDATA_WIDTH = "8" *) 
  (* C_RESULT_TUSER_WIDTH = "1" *) 
  (* C_RESULT_WIDTH = "1" *) 
  (* C_THROTTLE_SCHEME = "3" *) 
  (* C_TLAST_RESOLUTION = "0" *) 
  (* C_XDEVICEFAMILY = "virtexuplus" *) 
  (* downgradeipidentifiedwarnings = "yes" *) 
  xlnx_fp_lt_floating_point_v7_1_6 U0
       (.aclk(1'b0),
        .aclken(1'b1),
        .aresetn(1'b1),
        .m_axis_result_tdata({NLW_U0_m_axis_result_tdata_UNCONNECTED[7:1],\^m_axis_result_tdata }),
        .m_axis_result_tlast(NLW_U0_m_axis_result_tlast_UNCONNECTED),
        .m_axis_result_tready(1'b0),
        .m_axis_result_tuser(NLW_U0_m_axis_result_tuser_UNCONNECTED[0]),
        .m_axis_result_tvalid(m_axis_result_tvalid),
        .s_axis_a_tdata(s_axis_a_tdata),
        .s_axis_a_tlast(1'b0),
        .s_axis_a_tready(NLW_U0_s_axis_a_tready_UNCONNECTED),
        .s_axis_a_tuser(1'b0),
        .s_axis_a_tvalid(s_axis_a_tvalid),
        .s_axis_b_tdata(s_axis_b_tdata),
        .s_axis_b_tlast(1'b0),
        .s_axis_b_tready(NLW_U0_s_axis_b_tready_UNCONNECTED),
        .s_axis_b_tuser(1'b0),
        .s_axis_b_tvalid(s_axis_b_tvalid),
        .s_axis_c_tdata({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axis_c_tlast(1'b0),
        .s_axis_c_tready(NLW_U0_s_axis_c_tready_UNCONNECTED),
        .s_axis_c_tuser(1'b0),
        .s_axis_c_tvalid(1'b0),
        .s_axis_operation_tdata({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axis_operation_tlast(1'b0),
        .s_axis_operation_tready(NLW_U0_s_axis_operation_tready_UNCONNECTED),
        .s_axis_operation_tuser(1'b0),
        .s_axis_operation_tvalid(1'b0));
endmodule

(* C_ACCUM_INPUT_MSB = "32" *) (* C_ACCUM_LSB = "-31" *) (* C_ACCUM_MSB = "32" *) 
(* C_A_FRACTION_WIDTH = "24" *) (* C_A_TDATA_WIDTH = "32" *) (* C_A_TUSER_WIDTH = "1" *) 
(* C_A_WIDTH = "32" *) (* C_BRAM_USAGE = "0" *) (* C_B_FRACTION_WIDTH = "24" *) 
(* C_B_TDATA_WIDTH = "32" *) (* C_B_TUSER_WIDTH = "1" *) (* C_B_WIDTH = "32" *) 
(* C_COMPARE_OPERATION = "1" *) (* C_C_FRACTION_WIDTH = "24" *) (* C_C_TDATA_WIDTH = "32" *) 
(* C_C_TUSER_WIDTH = "1" *) (* C_C_WIDTH = "32" *) (* C_FIXED_DATA_UNSIGNED = "0" *) 
(* C_HAS_ABSOLUTE = "0" *) (* C_HAS_ACCUMULATOR_A = "0" *) (* C_HAS_ACCUMULATOR_S = "0" *) 
(* C_HAS_ACCUM_INPUT_OVERFLOW = "0" *) (* C_HAS_ACCUM_OVERFLOW = "0" *) (* C_HAS_ACLKEN = "0" *) 
(* C_HAS_ADD = "0" *) (* C_HAS_ARESETN = "0" *) (* C_HAS_A_TLAST = "0" *) 
(* C_HAS_A_TUSER = "0" *) (* C_HAS_B = "1" *) (* C_HAS_B_TLAST = "0" *) 
(* C_HAS_B_TUSER = "0" *) (* C_HAS_C = "0" *) (* C_HAS_COMPARE = "1" *) 
(* C_HAS_C_TLAST = "0" *) (* C_HAS_C_TUSER = "0" *) (* C_HAS_DIVIDE = "0" *) 
(* C_HAS_DIVIDE_BY_ZERO = "0" *) (* C_HAS_EXPONENTIAL = "0" *) (* C_HAS_FIX_TO_FLT = "0" *) 
(* C_HAS_FLT_TO_FIX = "0" *) (* C_HAS_FLT_TO_FLT = "0" *) (* C_HAS_FMA = "0" *) 
(* C_HAS_FMS = "0" *) (* C_HAS_INVALID_OP = "0" *) (* C_HAS_LOGARITHM = "0" *) 
(* C_HAS_MULTIPLY = "0" *) (* C_HAS_OPERATION = "0" *) (* C_HAS_OPERATION_TLAST = "0" *) 
(* C_HAS_OPERATION_TUSER = "0" *) (* C_HAS_OVERFLOW = "0" *) (* C_HAS_RECIP = "0" *) 
(* C_HAS_RECIP_SQRT = "0" *) (* C_HAS_RESULT_TLAST = "0" *) (* C_HAS_RESULT_TUSER = "0" *) 
(* C_HAS_SQRT = "0" *) (* C_HAS_SUBTRACT = "0" *) (* C_HAS_UNDERFLOW = "0" *) 
(* C_LATENCY = "0" *) (* C_MULT_USAGE = "0" *) (* C_OPERATION_TDATA_WIDTH = "8" *) 
(* C_OPERATION_TUSER_WIDTH = "1" *) (* C_OPTIMIZATION = "1" *) (* C_RATE = "1" *) 
(* C_RESULT_FRACTION_WIDTH = "0" *) (* C_RESULT_TDATA_WIDTH = "8" *) (* C_RESULT_TUSER_WIDTH = "1" *) 
(* C_RESULT_WIDTH = "1" *) (* C_THROTTLE_SCHEME = "3" *) (* C_TLAST_RESOLUTION = "0" *) 
(* C_XDEVICEFAMILY = "virtexuplus" *) (* ORIG_REF_NAME = "floating_point_v7_1_6" *) (* downgradeipidentifiedwarnings = "yes" *) 
module xlnx_fp_lt_floating_point_v7_1_6
   (aclk,
    aclken,
    aresetn,
    s_axis_a_tvalid,
    s_axis_a_tready,
    s_axis_a_tdata,
    s_axis_a_tuser,
    s_axis_a_tlast,
    s_axis_b_tvalid,
    s_axis_b_tready,
    s_axis_b_tdata,
    s_axis_b_tuser,
    s_axis_b_tlast,
    s_axis_c_tvalid,
    s_axis_c_tready,
    s_axis_c_tdata,
    s_axis_c_tuser,
    s_axis_c_tlast,
    s_axis_operation_tvalid,
    s_axis_operation_tready,
    s_axis_operation_tdata,
    s_axis_operation_tuser,
    s_axis_operation_tlast,
    m_axis_result_tvalid,
    m_axis_result_tready,
    m_axis_result_tdata,
    m_axis_result_tuser,
    m_axis_result_tlast);
  input aclk;
  input aclken;
  input aresetn;
  input s_axis_a_tvalid;
  output s_axis_a_tready;
  input [31:0]s_axis_a_tdata;
  input [0:0]s_axis_a_tuser;
  input s_axis_a_tlast;
  input s_axis_b_tvalid;
  output s_axis_b_tready;
  input [31:0]s_axis_b_tdata;
  input [0:0]s_axis_b_tuser;
  input s_axis_b_tlast;
  input s_axis_c_tvalid;
  output s_axis_c_tready;
  input [31:0]s_axis_c_tdata;
  input [0:0]s_axis_c_tuser;
  input s_axis_c_tlast;
  input s_axis_operation_tvalid;
  output s_axis_operation_tready;
  input [7:0]s_axis_operation_tdata;
  input [0:0]s_axis_operation_tuser;
  input s_axis_operation_tlast;
  output m_axis_result_tvalid;
  input m_axis_result_tready;
  output [7:0]m_axis_result_tdata;
  output [0:0]m_axis_result_tuser;
  output m_axis_result_tlast;

  wire \<const0> ;
  wire [0:0]\^m_axis_result_tdata ;
  wire m_axis_result_tvalid;
  wire [31:0]s_axis_a_tdata;
  wire s_axis_a_tvalid;
  wire [31:0]s_axis_b_tdata;
  wire s_axis_b_tvalid;
  wire NLW_i_synth_m_axis_result_tlast_UNCONNECTED;
  wire NLW_i_synth_s_axis_a_tready_UNCONNECTED;
  wire NLW_i_synth_s_axis_b_tready_UNCONNECTED;
  wire NLW_i_synth_s_axis_c_tready_UNCONNECTED;
  wire NLW_i_synth_s_axis_operation_tready_UNCONNECTED;
  wire [7:1]NLW_i_synth_m_axis_result_tdata_UNCONNECTED;
  wire [0:0]NLW_i_synth_m_axis_result_tuser_UNCONNECTED;

  assign m_axis_result_tdata[7] = \<const0> ;
  assign m_axis_result_tdata[6] = \<const0> ;
  assign m_axis_result_tdata[5] = \<const0> ;
  assign m_axis_result_tdata[4] = \<const0> ;
  assign m_axis_result_tdata[3] = \<const0> ;
  assign m_axis_result_tdata[2] = \<const0> ;
  assign m_axis_result_tdata[1] = \<const0> ;
  assign m_axis_result_tdata[0] = \^m_axis_result_tdata [0];
  assign m_axis_result_tlast = \<const0> ;
  assign m_axis_result_tuser[0] = \<const0> ;
  assign s_axis_a_tready = \<const0> ;
  assign s_axis_b_tready = \<const0> ;
  assign s_axis_c_tready = \<const0> ;
  assign s_axis_operation_tready = \<const0> ;
  GND GND
       (.G(\<const0> ));
  (* C_ACCUM_INPUT_MSB = "32" *) 
  (* C_ACCUM_LSB = "-31" *) 
  (* C_ACCUM_MSB = "32" *) 
  (* C_A_FRACTION_WIDTH = "24" *) 
  (* C_A_TDATA_WIDTH = "32" *) 
  (* C_A_TUSER_WIDTH = "1" *) 
  (* C_A_WIDTH = "32" *) 
  (* C_BRAM_USAGE = "0" *) 
  (* C_B_FRACTION_WIDTH = "24" *) 
  (* C_B_TDATA_WIDTH = "32" *) 
  (* C_B_TUSER_WIDTH = "1" *) 
  (* C_B_WIDTH = "32" *) 
  (* C_COMPARE_OPERATION = "1" *) 
  (* C_C_FRACTION_WIDTH = "24" *) 
  (* C_C_TDATA_WIDTH = "32" *) 
  (* C_C_TUSER_WIDTH = "1" *) 
  (* C_C_WIDTH = "32" *) 
  (* C_FIXED_DATA_UNSIGNED = "0" *) 
  (* C_HAS_ABSOLUTE = "0" *) 
  (* C_HAS_ACCUMULATOR_A = "0" *) 
  (* C_HAS_ACCUMULATOR_S = "0" *) 
  (* C_HAS_ACCUM_INPUT_OVERFLOW = "0" *) 
  (* C_HAS_ACCUM_OVERFLOW = "0" *) 
  (* C_HAS_ACLKEN = "0" *) 
  (* C_HAS_ADD = "0" *) 
  (* C_HAS_ARESETN = "0" *) 
  (* C_HAS_A_TLAST = "0" *) 
  (* C_HAS_A_TUSER = "0" *) 
  (* C_HAS_B = "1" *) 
  (* C_HAS_B_TLAST = "0" *) 
  (* C_HAS_B_TUSER = "0" *) 
  (* C_HAS_C = "0" *) 
  (* C_HAS_COMPARE = "1" *) 
  (* C_HAS_C_TLAST = "0" *) 
  (* C_HAS_C_TUSER = "0" *) 
  (* C_HAS_DIVIDE = "0" *) 
  (* C_HAS_DIVIDE_BY_ZERO = "0" *) 
  (* C_HAS_EXPONENTIAL = "0" *) 
  (* C_HAS_FIX_TO_FLT = "0" *) 
  (* C_HAS_FLT_TO_FIX = "0" *) 
  (* C_HAS_FLT_TO_FLT = "0" *) 
  (* C_HAS_FMA = "0" *) 
  (* C_HAS_FMS = "0" *) 
  (* C_HAS_INVALID_OP = "0" *) 
  (* C_HAS_LOGARITHM = "0" *) 
  (* C_HAS_MULTIPLY = "0" *) 
  (* C_HAS_OPERATION = "0" *) 
  (* C_HAS_OPERATION_TLAST = "0" *) 
  (* C_HAS_OPERATION_TUSER = "0" *) 
  (* C_HAS_OVERFLOW = "0" *) 
  (* C_HAS_RECIP = "0" *) 
  (* C_HAS_RECIP_SQRT = "0" *) 
  (* C_HAS_RESULT_TLAST = "0" *) 
  (* C_HAS_RESULT_TUSER = "0" *) 
  (* C_HAS_SQRT = "0" *) 
  (* C_HAS_SUBTRACT = "0" *) 
  (* C_HAS_UNDERFLOW = "0" *) 
  (* C_LATENCY = "0" *) 
  (* C_MULT_USAGE = "0" *) 
  (* C_OPERATION_TDATA_WIDTH = "8" *) 
  (* C_OPERATION_TUSER_WIDTH = "1" *) 
  (* C_OPTIMIZATION = "1" *) 
  (* C_RATE = "1" *) 
  (* C_RESULT_FRACTION_WIDTH = "0" *) 
  (* C_RESULT_TDATA_WIDTH = "8" *) 
  (* C_RESULT_TUSER_WIDTH = "1" *) 
  (* C_RESULT_WIDTH = "1" *) 
  (* C_THROTTLE_SCHEME = "3" *) 
  (* C_TLAST_RESOLUTION = "0" *) 
  (* C_XDEVICEFAMILY = "virtexuplus" *) 
  (* downgradeipidentifiedwarnings = "yes" *) 
  xlnx_fp_lt_floating_point_v7_1_6_viv i_synth
       (.aclk(1'b0),
        .aclken(1'b0),
        .aresetn(1'b0),
        .m_axis_result_tdata({NLW_i_synth_m_axis_result_tdata_UNCONNECTED[7:1],\^m_axis_result_tdata }),
        .m_axis_result_tlast(NLW_i_synth_m_axis_result_tlast_UNCONNECTED),
        .m_axis_result_tready(1'b0),
        .m_axis_result_tuser(NLW_i_synth_m_axis_result_tuser_UNCONNECTED[0]),
        .m_axis_result_tvalid(m_axis_result_tvalid),
        .s_axis_a_tdata(s_axis_a_tdata),
        .s_axis_a_tlast(1'b0),
        .s_axis_a_tready(NLW_i_synth_s_axis_a_tready_UNCONNECTED),
        .s_axis_a_tuser(1'b0),
        .s_axis_a_tvalid(s_axis_a_tvalid),
        .s_axis_b_tdata(s_axis_b_tdata),
        .s_axis_b_tlast(1'b0),
        .s_axis_b_tready(NLW_i_synth_s_axis_b_tready_UNCONNECTED),
        .s_axis_b_tuser(1'b0),
        .s_axis_b_tvalid(s_axis_b_tvalid),
        .s_axis_c_tdata({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axis_c_tlast(1'b0),
        .s_axis_c_tready(NLW_i_synth_s_axis_c_tready_UNCONNECTED),
        .s_axis_c_tuser(1'b0),
        .s_axis_c_tvalid(1'b0),
        .s_axis_operation_tdata({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axis_operation_tlast(1'b0),
        .s_axis_operation_tready(NLW_i_synth_s_axis_operation_tready_UNCONNECTED),
        .s_axis_operation_tuser(1'b0),
        .s_axis_operation_tvalid(1'b0));
endmodule
`pragma protect begin_protected
`pragma protect version = 1
`pragma protect encrypt_agent = "XILINX"
`pragma protect encrypt_agent_info = "Xilinx Encryption Tool 2015"
`pragma protect key_keyowner="Cadence Design Systems.", key_keyname="cds_rsa_key", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=64)
`pragma protect key_block
TTgOL/yc8scnx5F26iaQhttGRbfvRCqLvIJus68zAJzHMoRI+yW+zuwHu7vJOLMSOWdVfoE6K18s
HgglcaIRdg==

`pragma protect key_keyowner="Synopsys", key_keyname="SNPS-VCS-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
cKn4kmjDn+2Rn+bq5QAuSfkpjwIkpCjPMrW3nl6687/gNX+f8ocwUbkw/w7emiznZu6X9GaLhfrQ
RW1lWZlRJ3U+ueLvsn3x8PG7hHf8/HfJafrzTzWu/GMiU7tg+TVS83dx/2r08uJs0gkFPoBv17sk
G30KHUntxIih0tAw9Yk=

`pragma protect key_keyowner="Aldec", key_keyname="ALDEC15_001", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
kXUcLSepR1yM2EYG7sXLQpMMEjMlbLQYz6L9LfSCaLPAt5NQ9gxMCplHmDs0NOy5O3zEOBc6K/mH
yisdkkKlh2SRnDfrOXavxWeVx7XVPJ/3iol+DDB6Ena1M4le+cRChHIw2eWOsZCafdbyCYzHpq8W
zEWuf/Doi1KJK7R2Q2H+RklPx7ITPQe2wzxojKfy9PqRkFLMVxem6YDcoY5BdPmn3Fw5oz5uzLXo
37rWhaxiOx0HOFs3KagtvBVBUOlAh7L5b0miUfr3lCFwjmrVOoog/dKUZWt4zd7ZGDYrfcTXfWfi
qEiqL+KxKRAOXMIxNxCRkSFf6zIRFvJ498NFKw==

`pragma protect key_keyowner="ATRENTA", key_keyname="ATR-SG-2015-RSA-3", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
s/rYlYkK17IK0IsJA0qYUXfPuqVL2O4FaulvnnVNlhfUY2naHBQNB13yZlgHQmidslwn5lGN/O0F
ByHna7JskuuCiSyc0m87UX3fo87Nj0Uq9YtUlSXJDbeZ5OlL9XzXbHznvYnCqAkZIHmeZr9Elrxv
DrRds9Ns8ZvuS6mZiy4AtdJViBYhHMxyKDt/rMdSoIubQIOKD1wY8rkaHcvEZxB3k1tRhWyloi0Y
glTZ2OqgjOzc42UQ7mXCVXKo0vrFYacqCluwMSihvgAgvZxsK/UScXOzmj7ugFWh5EYP6wVl58ZA
JPFniwer+OkV7hslvdbiGUx9bCbzN2VeMzy0Rg==

`pragma protect key_keyowner="Xilinx", key_keyname="xilinxt_2017_05", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
XTebD6awsbI94nxEkr8hX1cPSSvFwsuh8IFQij+C9qS83quz3dei1lngzClNXJTuaCDViPmZxq6p
OTsIlIOcyB/YDP/GTrTvTu/7xbmCB0BC9Rh888b9yUCbGAhXfRsDAgwhEw0j32fXtY3qNgor3MDE
EooAnTKnW6rnWiW/zaulmZIaJ//RiW7JtVWnVqFhm+S+E1F/9hmIYo4H2y4kiBWP87TwBYUREJ3s
aj9xZXp/d8vVkKR27E5YdR2puRX5rz/2UpXWR/DfdIaw1IerW5r5Ff/NiEBJaWzyUmuIhJ/CIYiU
45vuC2ZMKEAYrLnFlqTrztmhm4KsZeDAEuu9cw==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VELOCE-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
STDvZwcfjhfpj/Usps4nXa7/z/Y3PWXcMUxcKMZd6Jy1kZkAP5w3xkS9ZFlEe5gImUPtEtc9A/i5
OhwgZAd7/fe7XldWY7V8uWm/8A4NtVfTw0HTxdsxHLAqli7T7BMGysl4K63jLph8wtZva5Uae35l
g90k3X1Emkm2YHdIjqQ=

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VERIF-SIM-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
XjctUbQp40FO/D04Oo1TDWxrryFqbI++Aom6QrXQy2n0Ah+2PHjciCMnZSr1Lg+KDjcNtidMohXK
xFLdRAnBItXBJd7nbm9/wQQ8du6NEj8wFYnnGv1YtYCmHb58qmFmqe4xOMtDbQGZz/KiF2N2Kkph
wnt3stoKx+fiEbD2jOX0jQ7JyRivWr+fN/Xdj5Mfu1LzMM1zOXQC1R4UMCC5c5dQ3UGCeoBAbwF5
zwEDYsG81xQmtQ6rGvGYsdKROfvbv2q31kAX/SuJAjq8zDuwpJ37AjLQQBopBfXM5na/e1T0JDvT
EAXqlZr6CajnDRurSnc6RcypdulvYqlfSt0toA==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-PREC-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
mZgI8UfgkgOQICFTUKkvCcBZBdWHAUTfdXz2GuMGMNa54mqUDWG2wp81rRd1FlrPS1IK0OKM2GG5
RYxh4GO2N734lR4xBorPY/RINdKjxlp+jwrKrOqTn7U4E7IcY6YDPuVn72fUa9VQOOPRH630Lt3f
QCKIOY0WEn/CuQzpSb296mru+nqcPgAevXTOlyiJlzuCfOLOFjq7hK9QIUUmB+GrsBawivZ7F/DS
FnqOrV6y0ORNbj2yKRWwCwO40ORWWb5FrnSJcA5VH2vxcqEOr3xkMpVWb+1hajd3p+Mh/hCx+6dR
OVbNnj+4vvNY3MCCaaotCuozdhDuff4e6mf5kQ==

`pragma protect key_keyowner="Synplicity", key_keyname="SYNP15_1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
NizvuSf0Nvcg/wYM0yNeG1TxKON/k92BXCRC7nBIPHSoo6gXHPKCMr8YlgECvuGp2bi8LhbuLV46
yM+aSa2B6icaAdalWhm1v5cdho5Wrb+Z3IeC8H6QTAvy63yTuArlidwYC9/wvJVY0lfVQWcdzaT4
Gy5/0ccxih+f6sq1+/IWG8PbNiYfTgHhbHXupOACbygkk89Q0RK2YHhQQ4IwUg5xTDm/UBSaz7/e
an6oNhhANLXvIAlzeZIIwNBwLokFhGcObqUleM+4VGCRzc3FBfe3y56TP3z45XvLKOsPhzyfkYnG
s2zal61QNqeLmh61G2fCWdN0OaRA8frwURCuiQ==

`pragma protect data_method = "AES128-CBC"
`pragma protect encoding = (enctype = "BASE64", line_length = 76, bytes = 43120)
`pragma protect data_block
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
eMD8P5aCNNAqVOXNfA+a6CL5/7fYQ5n+/+G/SA==
`pragma protect end_protected
`ifndef GLBL
`define GLBL
`timescale  1 ps / 1 ps

module glbl ();

    parameter ROC_WIDTH = 100000;
    parameter TOC_WIDTH = 0;

//--------   STARTUP Globals --------------
    wire GSR;
    wire GTS;
    wire GWE;
    wire PRLD;
    tri1 p_up_tmp;
    tri (weak1, strong0) PLL_LOCKG = p_up_tmp;

    wire PROGB_GLBL;
    wire CCLKO_GLBL;
    wire FCSBO_GLBL;
    wire [3:0] DO_GLBL;
    wire [3:0] DI_GLBL;
   
    reg GSR_int;
    reg GTS_int;
    reg PRLD_int;

//--------   JTAG Globals --------------
    wire JTAG_TDO_GLBL;
    wire JTAG_TCK_GLBL;
    wire JTAG_TDI_GLBL;
    wire JTAG_TMS_GLBL;
    wire JTAG_TRST_GLBL;

    reg JTAG_CAPTURE_GLBL;
    reg JTAG_RESET_GLBL;
    reg JTAG_SHIFT_GLBL;
    reg JTAG_UPDATE_GLBL;
    reg JTAG_RUNTEST_GLBL;

    reg JTAG_SEL1_GLBL = 0;
    reg JTAG_SEL2_GLBL = 0 ;
    reg JTAG_SEL3_GLBL = 0;
    reg JTAG_SEL4_GLBL = 0;

    reg JTAG_USER_TDO1_GLBL = 1'bz;
    reg JTAG_USER_TDO2_GLBL = 1'bz;
    reg JTAG_USER_TDO3_GLBL = 1'bz;
    reg JTAG_USER_TDO4_GLBL = 1'bz;

    assign (strong1, weak0) GSR = GSR_int;
    assign (strong1, weak0) GTS = GTS_int;
    assign (weak1, weak0) PRLD = PRLD_int;

    initial begin
	GSR_int = 1'b1;
	PRLD_int = 1'b1;
	#(ROC_WIDTH)
	GSR_int = 1'b0;
	PRLD_int = 1'b0;
    end

    initial begin
	GTS_int = 1'b1;
	#(TOC_WIDTH)
	GTS_int = 1'b0;
    end

endmodule
`endif
