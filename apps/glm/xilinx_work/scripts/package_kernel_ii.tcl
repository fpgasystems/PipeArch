# /*******************************************************************************
# Copyright (c) 2018, Xilinx, Inc.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 
# 
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
# 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# *******************************************************************************/

set path_to_hdl1    "../hw/common"
set path_to_hdl2    "../hw/xilinx_top/"
set path_to_hdl3    "../../../common-rtl/"
set path_to_hdl4    "../../../common-rtl/xilinx_dma"
set path_to_hdl5    "../../../common-rtl/compute"
set path_to_hdl6    "../../../common-rtl/memory"
set path_to_ip      "../../../common-rtl/IP/xilinx_IP/"
set path_to_packaged "./packaged_kernel_ii_${suffix}"
set path_to_tmp_project "./tmp_kernel_pack_ii_${suffix}"

create_project -force kernel_pack $path_to_tmp_project -part xcvu9p-fsgd2104-2-i
add_files -norecurse [glob $path_to_hdl1/*.v $path_to_hdl1/*.sv $path_to_hdl1/*.vhd $path_to_hdl1/*.vh]
add_files -norecurse [glob $path_to_hdl2/xilinx_top_ii.v $path_to_hdl2/xilinx_glm_top.sv]
add_files -norecurse [glob $path_to_hdl3/*.v $path_to_hdl3/*.sv $path_to_hdl3/*.vhd $path_to_hdl3/*.vh]
add_files -norecurse [glob $path_to_hdl4/*.v $path_to_hdl4/*.sv $path_to_hdl4/*.vhd $path_to_hdl4/*.vh]
add_files -norecurse [glob $path_to_hdl5/*.v $path_to_hdl5/*.sv $path_to_hdl5/*.vhd $path_to_hdl5/*.vh]
add_files -norecurse [glob $path_to_hdl6/*.v $path_to_hdl6/*.sv $path_to_hdl6/*.vhd $path_to_hdl6/*.vh]
add_files -norecurse [glob $path_to_ip/*/*.xci]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
ipx::package_project -root_dir $path_to_packaged -vendor xilinx.com -library RTLKernel -taxonomy /KernelIP -import_files -set_current false
ipx::unload_core $path_to_packaged/component.xml
ipx::edit_ip_in_project -upgrade true -name tmp_edit_project -directory $path_to_packaged $path_to_packaged/component.xml
set_property core_revision 2 [ipx::current_core]
foreach up [ipx::get_user_parameters] {
  ipx::remove_user_parameter [get_property NAME $up] [ipx::current_core]
}
set_property sdx_kernel true [ipx::current_core]
set_property sdx_kernel_type rtl [ipx::current_core]
ipx::create_xgui_files [ipx::current_core]
ipx::associate_bus_interfaces -busif m_axi_gmem -clock ap_clk [ipx::current_core]
ipx::associate_bus_interfaces -busif s_axi_control -clock ap_clk [ipx::current_core]
set_property xpm_libraries {XPM_CDC XPM_MEMORY XPM_FIFO} [ipx::current_core]
set_property supported_families { } [ipx::current_core]
set_property auto_family_support_level level_2 [ipx::current_core]
ipx::update_checksums [ipx::current_core]
ipx::save_core [ipx::current_core]
close_project -delete
