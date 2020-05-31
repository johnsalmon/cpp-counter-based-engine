# The C++ code uses concepts, so CXX should point to gcc-10.
#
# use -std=gnu++20 so numeric_limits<__uint128_t> "works"
CXXFLAGS+=-std=gnu++2a -Wall
CXXFLAGS+=-O3
#CXXFLAGS+=-O0 -ggdb
TARGET_ARCH+=-pthread # for philoxbench

all: philoxexample tests bench

threefry.o : CPPFLAGS+=-I/u/nyc/salmonj/g/gardenfs/core123/include

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

CPPSRCS:=$(wildcard *.cpp)
DEPFILES := $(CSRCS:%.c=$(DEPDIR)/%.d) $(CPPSRCS:%.cpp=$(DEPDIR)/%.d)
$(DEPFILES):
include $(wildcard $(DEPFILES))
# </autodepends>
