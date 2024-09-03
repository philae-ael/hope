BUILD_TYPE?=Debug
BUILD_SHARED_LIBS?=ON
OUTPUT_DIR?=build
CC:=gcc
CXX:=g++

.PHONY: all run clean format iwyu build-all

all: build-all

build-all: $(OUTPUT_DIR)/build.ninja
	ninja -C$(OUTPUT_DIR) 

run:  build-all
	cd $(OUTPUT_DIR); ./loader

clean:
	rm -rf ./$(OUTPUT_DIR)

format:
	./scripts/format-all.sh

iwyu: 
	iwyu-tool -p ./compile_commands.json -- -Xiwyu --mapping_file=$(PWD)/iwyu-mapping.yaml

cmake: $(OUTPUT_DIR)/build.ninja

$(OUTPUT_DIR)/build.ninja: CMakeLists.txt
	cmake -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_CXX_FLAGS="$(CXXFLAGS)" -DCMAKE_C_FLAGS="$(CFLAGS)" -B$(OUTPUT_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -GNinja  -DBUILD_SHARED_LIBS=$(BUILD_SHARED_LIBS)
