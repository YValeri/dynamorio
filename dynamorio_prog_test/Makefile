CC=g++
SOURCE=$(wildcard *.cpp) 
OUT=$(SOURCE:%.cpp=%.out)
FLAG=-g -O3

all: $(OUT)

call.out: call.cpp
	$(CC) -o call.out call.cpp -march=native -fopenmp $(FLAG)

commutativity.out: commutativity.cpp
	$(CC) -o commutativity.out commutativity.cpp $(FLAG)

divtest.out: divtest.cpp
	$(CC) -o divtest.out divtest.cpp $(FLAG) -masm=intel

matmul_blas.out: matmul_blas.cpp
	$(CC) -o matmul_blas.out matmul_blas.cpp -lopenblas $(FLAG)

rump.out: rump.cpp
	$(CC) -o rump.out rump.cpp $(FLAG)

test_instructions_sse.out: test_instructions.h test_instructions_sse.cpp
	$(CC) -o test_instructions_sse.out test_instructions_sse.cpp -msse $(FLAG)

test_instructions_avx.out: test_instructions.h test_instructions_avx.cpp
	$(CC) -o test_instructions_avx.out test_instructions_avx.cpp -mavx $(FLAG)

test_instructions_fma.out: test_instructions.h test_instructions_fma.cpp
	$(CC) -o test_instructions_fma.out test_instructions_fma.cpp -mfma $(FLAG)


.PHONY: clean

clean:
	rm $(OUT)