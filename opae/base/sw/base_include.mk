##
## Define base source files
##
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(dir $(mkfile_path))

include $(current_dir)/../../common/sw/common_include.mk

BASE_FILE_PATH = $(current_dir)
BASE_FILE_SRC = opae_svc_wrapper.cpp
BASE_FILE_INC = $(BASE_FILE_PATH)/opae_svc_wrapper.h $(BASE_FILE_PATH)/csr_mgr.h

VPATH = .:$(BASE_FILE_PATH)

CPPFLAGS += -I$(BASE_FILE_PATH)
LDFLAGS += -lboost_program_options -lMPF-cxx -lMPF -lopae-cxx-core -lpthread
