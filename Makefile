CC=gcc
CXX=g++

OBJDIR := build
DEPDIR := build

CXXFLAGS := $(CXXFLAGS) -O0 -g -std=c++20
CXXFLAGS := $(CXXFLAGS) -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CXXFLAGS := $(CXXFLAGS) -DDEBUG
# CXXFLAGS := $(CXXFLAGS) -DARENA_DEBUG 
# CXXFLAGS := $(CXXFLAGS) -DSCRATCH_DEBUG 
# CXXFLAGS := $(CXXFLAGS) -fsanitize=address 

# clang only
# CXXFLAGS := $(CXXFLAGS) -ftime-trace
 	
LDFLAGS:=$(LDFLAGS) -rdynamic 
LDLIBS:=$(LDLIBS) -lstdc++exp 

DEPFLAGS= -MM -MG -MF 

SRCS := main.cpp
-include core/core.mk

OBJS := $(patsubst %,$(OBJDIR)/%.o,$(basename $(SRCS)))
DEPENDS := $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS)))
OUTPUT := main

.SUFFIXES:
.PHONY: all clean run

all:$(OUTPUT)

$(OUTPUT): $(OBJS) 
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $^ $(LDLIBS) -o $@ 

$(OBJDIR)/%.o: %.cpp Makefile
	mkdir -p $(dir $@)
	$(CXX) -c $< $(CXXFLAGS) -o $@

clean:
	@rm -r $(OBJDIR)
	@rm $(OUTPUT)

run: $(OUTPUT)
	./$(OUTPUT)

$(DEPDIR)/%.d:: %.cpp Makefile
	mkdir -p $(dir $@)
	$(CXX) $(DEPFLAGS) $@ $< -MP -MT $(<:%.cpp=$(OBJDIR)/%.o)

include  $(DEPENDS)
