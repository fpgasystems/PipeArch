COMMON_REPO := ${SDACCEL_EXAMPLES}

hw_path = ../hw
sw_path = ../sw

#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

# Host Application
TEST=netflix

LOCAL_SRCS=$(wildcard $(sw_path)/src/*.cpp)
LOCAL_TESTS=$(sw_path)/tests/$(TEST).cpp
LOCAL_DEPS=$(wildcard $(sw_path)/include/*.h)

host_SRCS=$(LOCAL_SRCS) $(LOCAL_TESTS) $(xcl2_SRCS)
host_HDRS=$(LOCAL_DEPS) $(xcl2_HDRS)
host_CXXFLAGS=-I$(sw_path)/include $(opencl_CXXFLAGS) $(xcl2_CXXFLAGS) -march=native -DXILINX -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
host_CXXFLAGS+=-DMAKEFILE_MAX_NUM_INSTANCES=4 -DMAKEFILE_NUM_INSTANCES_PER_BANK=4
host_LDFLAGS=$(opencl_LDFLAGS)
EXES=host

mytop_KERNEL:=xilinx_top

# RTL Kernel Sources
mytop_HDLSRCS=$(hw_path)/xilinx_top/kernel.xml\
						scripts/package_kernel.tcl\
						scripts/gen_xo.tcl
mytop_TCL=scripts/gen_xo.tcl

RTLXOS=mytop

# Kernel
mytop_XOS=mytop
mytop_NTARGETS=sw_emu

XCLBINS=mytop
EXTRA_CLEAN=tmp_kernel_pack* packaged_kernel* $(mytop_KERNEL).xo

# check
check_EXE=host
check_XCLBINS=mytop
check_NTARGETS=$(mytop_NTARGETS)

TARGETS=hw
CHECKS=check

#Reporting warning if targeting for sw_emu
ifneq (,$(findstring sw_emu,$(TARGETS)))
$(warning WARNING:RTL Kernels do not support sw_emu TARGETS. Please use hw_emu for running RTL kernel Emulation)
endif

# For waveform debugging
LDCLFLAGS+=-g --nk $(mytop_KERNEL):4 #--sp $(mytop_KERNEL)_1.m_axi_gmem:bank0 --sp $(mytop_KERNEL)_2.m_axi_gmem:bank1

include $(COMMON_REPO)/utility/rules.mk

all:
	emconfigutil --platform 'xilinx:vcu1525:dynamic' --nd 1