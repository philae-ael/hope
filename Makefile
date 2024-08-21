CC=clang
CXX=clang++

OBJDIR := build
DEPDIR := build

CXXFLAGS:=$(CXXFLAGS) -O3 -g -std=c++23
CXXFLAGS:=$(CXXFLAGS) -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CXXFLAGS:=$(CXXFLAGS) -DDEBUG
CXXFLAGS:=$(CXXFLAGS) -DARENA_DEBUG 
CXXFLAGS:=$(CXXFLAGS) -DMEM_DEBUG 
CXXFLAGS:=$(CXXFLAGS) -DMEM_USE_MALLOC
CXXFLAGS:=$(CXXFLAGS) -DSCRATCH_DEBUG 
CXXFLAGS:=$(CXXFLAGS) -fno-omit-frame-pointer
# CXXFLAGS:=$(CXXFLAGS) -fsanitize=address 
# CXXFLAGS:=$(CXXFLAGS) -flto

# clang only
# CXXFLAGS:=$(CXXFLAGS) -ftime-trace
 	
LDFLAGS:=$(LDFLAGS) -rdynamic 
LDLIBS:=$(LDLIBS) -lstdc++exp 

DEPFLAGS= -MM -MG -MF 

SRCS := src/main.cpp
-include src/core/core.mk
-include src/os/os.mk

OBJS := $(patsubst %,$(OBJDIR)/%.o,$(basename $(SRCS)))
DEPENDS := $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS)))
OUTPUT := main

.SUFFIXES:
.PHONY: all clean run doc open-doc open-doc-html doc-watch

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

doc:
	doxygen

./build/doc/latex/refman.pdf: doc
	make -C ./build/doc/latex

open-doc-html: doc
	xdg-open ./build/doc/html/index.html &

doc-watch: doc
	browser-sync start -w --ss ./build/doc/html/ -s ./build/doc/html/ --directory

open-doc: ./build/doc/latex/refman.pdf
	evince $< &


compile_commands.json: Makefile
	echo "regenerating compile_commands.json"
	bear -- make -B

include  $(DEPENDS)
