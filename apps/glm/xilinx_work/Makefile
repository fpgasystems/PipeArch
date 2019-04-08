COMMON_REPO := ${SDACCEL_EXAMPLES}

sw_path = ../sw
hw_path = ../hw

#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

# Host Application
EXES=scratch

LOCAL_SRCS = $(wildcard $(sw_path)/src/*.cpp)
LOCAL_TESTS = $(sw_path)/tests/$(EXES).cpp
LOCAL_DEPS = $(wildcard $(sw_path)/include/*.h)

scratch_SRCS=$(LOCAL_SRCS) $(LOCAL_TESTS) $(xcl2_SRCS)
scratch_HDRS=$(LOCAL_DEPS) $(xcl2_HDRS)
scratch_CXXFLAGS=-I$(sw_path)/include $(opencl_CXXFLAGS) $(xcl2_CXXFLAGS) -march=native -DXILINX -Wno-unused-function -Wno-unused-variable
scratch_LDFLAGS=$(opencl_LDFLAGS)

mytop_KERNEL := xilinx_top

# RTL Kernel Sources
mytop_HDLSRCS= scripts/package_kernel.tcl\
			 scripts/gen_xo.tcl\
			 $(hw_path)/blueprint/kernel.xml\
			 ../../../common-rtl/*.v ../../../common-rtl/*.sv ../../../common-rtl/*.vhd
			 # ../../../common-rtl/compute/*.v ../../../common-rtl/compute/*.sv ../../../common-rtl/compute/*.vhd\
			 # ../../../common-rtl/memory/*.v ../../../common-rtl/memory/*.sv ../../../common-rtl/memory/*.vhd\
			 # ../../../common-rtl/IP/xilinx_IP/*.xci
mytop_TCL=scripts/gen_xo.tcl

RTLXOS=mytop

# Kernel
mytop_XOS=mytop
mytop_NTARGETS=hw_emu

XCLBINS=mytop
EXTRA_CLEAN=tmp_kernel_pack* packaged_kernel* $(mytop_KERNEL).xo

# check
check_EXE=scratch
check_XCLBINS=mytop
check_NTARGETS=$(mytop_NTARGETS)

CHECKS=check
TARGETS=hw_emu

#Reporting warning if targeting for sw_emu
ifneq (,$(findstring sw_emu,$(TARGETS)))
$(warning WARNING:RTL Kernels do not support sw_emu TARGETS. Please use hw_emu for running RTL kernel Emulation)
endif

$(info $$RTLXOS is [$(RTLXOS)])
$(info $$TARGETS is [$(TARGETS)])
$(info $$DEVICES is [$(DEVICES)])

include $(COMMON_REPO)/utility/rules.mk
