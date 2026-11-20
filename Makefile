# Alternativa a CMake: `make` compila, `make test` corre las pruebas.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC      := $(wildcard src/*.cpp)
BIN      := build/rvsim

all: $(BIN)

$(BIN): $(SRC) $(wildcard src/*.hpp)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

test: $(BIN)
	@sh tests/run_tests.sh $(BIN)

clean:
	rm -rf build

.PHONY: all test clean
