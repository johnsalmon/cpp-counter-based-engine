# The C++ code uses concepts, so CXX should point to gcc-10.
#
# use -std=gnu++20 so numeric_limits<__uint128_t> "works"
CXXFLAGS+=-std=gnu++2a -Wall
CXXFLAGS+=-fconcepts-diagnostics-depth=5
CFLAGS+=-Wall
OPT?=-O3 # if not set on the command line
CXXFLAGS+=$(OPT)
CFLAGS+=$(OPT)
TARGET_ARCH+=-pthread # for philoxbench
TARGET_ARCH+=-march=native

all: philoxexample tests bench

threefry.o : CPPFLAGS+=-I/u/nyc/salmonj/g/gardenfs/core123/include

bench : siphash.o

LINK.o = $(CXX) $(LDFLAGS) $(TARGET_ARCH)

# <autodepends from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation>
# Modified to work with CSRCS and CPPSRCS instead of just SRCS...
DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
% : %.c
%.o : %.c
%.o : %.c $(DEPDIR)/%.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

COMPILE.cpp = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
% : %.cpp
%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

$(DEPDIR): ; @mkdir -p $@

CSRCS:=$(wildcard *.c)
CPPSRCS:=$(wildcard *.cpp)
DEPFILES := $(CSRCS:%.c=$(DEPDIR)/%.d) $(CPPSRCS:%.cpp=$(DEPDIR)/%.d)
$(DEPFILES):
include $(wildcard $(DEPFILES))
# </autodepends>
