-- Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
-- --------------------------------------------------------------------------------
-- Tool Version: Vivado v.2018.2 (lin64) Build 2258646 Thu Jun 14 20:02:38 MDT 2018
-- Date        : Tue Apr  9 14:24:13 2019
-- Host        : kkara-desktop running 64-bit Ubuntu 16.04.5 LTS
-- Command     : write_vhdl -force -mode synth_stub
--               /home/kkara/Projects/aws-apps/rtl_vadd/_x/link/vivado/prj/prj.srcs/sources_1/ip/xlnx_dec23_to_fp_convert/xlnx_dec23_to_fp_convert_stub.vhdl
-- Design      : xlnx_dec23_to_fp_convert
-- Purpose     : Stub declaration of top-level module interface
-- Device      : xcvu9p-fsgd2104-2-i
-- --------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity xlnx_dec23_to_fp_convert is
  Port ( 
    aclk : in STD_LOGIC;
    s_axis_a_tvalid : in STD_LOGIC;
    s_axis_a_tdata : in STD_LOGIC_VECTOR ( 47 downto 0 );
    m_axis_result_tvalid : out STD_LOGIC;
    m_axis_result_tdata : out STD_LOGIC_VECTOR ( 31 downto 0 )
  );

end xlnx_dec23_to_fp_convert;

architecture stub of xlnx_dec23_to_fp_convert is
attribute syn_black_box : boolean;
attribute black_box_pad_pin : string;
attribute syn_black_box of stub : architecture is true;
attribute black_box_pad_pin of stub : architecture is "aclk,s_axis_a_tvalid,s_axis_a_tdata[47:0],m_axis_result_tvalid,m_axis_result_tdata[31:0]";
attribute x_core_info : string;
attribute x_core_info of stub : architecture is "floating_point_v7_1_6,Vivado 2018.2";
begin
end;
