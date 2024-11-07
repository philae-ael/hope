BUILD_TYPE:="Debug"
BUILD_SHARED_LIBS:="OFF"
OUTPUT_DIR:="build"
CC:="clang"
CXX:="clang++"
# CC:="cl"
# CXX:="cl"
CFLAGS:=""
CXXFLAGS:=""

default: build-all

build-loader: configure
	ninja -C{{OUTPUT_DIR}} loader app

build-all : configure
	ninja -C{{OUTPUT_DIR}}

build-tests: configure
	ninja -C{{OUTPUT_DIR}} testcore

run:  build-loader
	{{OUTPUT_DIR}}/loader

tests:  build-tests
	{{OUTPUT_DIR}}/testcore

[confirm("Are you sure you want to delete {{OUTPUT_DIR}} ?")]
clean:
	rm -rf ./{{OUTPUT_DIR}}

[unix]
format:
	./scripts/format-all.sh

[unix]
iwyu: 
	iwyu-tool -p ./compile_commands.json -- -Xiwyu --mapping_file=./iwyu-mapping.yaml

configure:
	cmake -DCMAKE_C_COMPILER={{CC}}  -DCMAKE_CXX_COMPILER={{CXX}} -DCMAKE_CXX_FLAGS="{{CXXFLAGS}}" -DCMAKE_C_FLAGS="{{CFLAGS}}" -B{{OUTPUT_DIR}} -DCMAKE_BUILD_TYPE={{BUILD_TYPE}} -GNinja  -DBUILD_SHARED_LIBS={{BUILD_SHARED_LIBS}}

