include ../../../opae/base/sw/base_include.mk

# Primary test name
TEST = main

# Build directory
OBJDIR = obj
INCLUDE = include
CFLAGS += -I./$(OBJDIR)
CPPFLAGS += -I./$(OBJDIR) -march=native

# Files and folders
LOCAL_SRCS = $(wildcard ./src/*.cpp)
SRCS = $(patsubst ./src/%,%,$(LOCAL_SRCS))
SRCS += $(BASE_FILE_SRC)
OBJS = $(addprefix $(OBJDIR)/,$(patsubst %.cpp,%.o,$(SRCS)))
DEPS = $(wildcard $(INCLUDE)/*.h)

$(info $$SRCS is [$(SRCS)])
$(info $$OBJS is [$(OBJS)])
$(info $$DEPS is [$(DEPS)])

# Targets
all: $(TEST) $(TEST)_ase

ex:
	export ASE_WORKDIR=../build_sim/work/

# AFU info from JSON file, including AFU UUID
AFU_JSON_INFO = $(OBJDIR)/afu_json_info.h
$(AFU_JSON_INFO): ../hw/rtl/pipearch_cci.json | objdir
	afu_json_mgr json-info --afu-json=$^ --c-hdr=$@
$(OBJS): $(AFU_JSON_INFO)

$(TEST): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(FPGA_LIBS)

$(TEST)_ase: $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(ASE_LIBS)

$(OBJDIR)/%.o: %.cpp $(DEPS) | objdir
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -I./$(INCLUDE)

$(OBJDIR)/%.o: ./src/%.cpp $(DEPS) | objdir
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -I./$(INCLUDE)

clean:
	rm -rf $(TEST) $(TEST)_ase $(OBJDIR)

objdir:
	@mkdir -p $(OBJDIR)

.PHONY: all clean
