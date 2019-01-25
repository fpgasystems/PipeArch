IDIR = ../../../codegen/src
TDIR = .
CC = g++
CPPFLAGS = -I$(IDIR) -march=native -O3 -std=c++11 -Wno-unused-result -g

DEPS = $(wildcard $(IDIR)/*.h)
SRCS = $(wildcard $(TDIR)/*.cpp)
EXE = $(patsubst %.cpp,%_gen,$(SRCS))

$(info $$DEPS is [$(DEPS)])
$(info $$SRCS is [$(SRCS)])
$(info $$EXE is [$(EXE)])

all: $(EXE)

%_gen: %.cpp $(DEPS)
	$(CC) -o $@ $(patsubst %_gen,%.cpp,$@) $(CPPFLAGS)

clean:
	rm $(EXE)