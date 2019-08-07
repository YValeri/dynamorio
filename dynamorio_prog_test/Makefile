CC=g++
SOURCE=$(wildcard *.cpp) 
OUT=$(SOURCE:%.cpp=%.out)
FLAG=-g -O3

all: $(OUT)

call.out: 
	$(CC) -o call.out call.cpp -march=native -fopenmp $(FLAG)

commutativity.out:
	$(CC) -o commutativity.out commutativity.cpp $(FLAG)

divtest.out:
	$(CC) -o divtest.out divtest.cpp $(FLAG)

matmul_blas.out:
	$(CC) -o matmul_blas.out matmul_blas.cpp -lopenblas $(FLAG)

rump.out:
	$(CC) -o rump.out rump.cpp $(FLAG)

test_instructions_sse.out: test_instructions.h
	$(CC) -o test_instructions_sse.out test_instructions_sse.cpp -msse $(FLAG)

test_instructions_avx.out: test_instructions.h
	$(CC) -o test_instructions_avx.out test_instructions_avx.cpp -mavx $(FLAG)

test_instructions_fma.out: test_instructions.h
	$(CC) -o test_instructions_fma.out test_instructions_fma.cpp -mfma $(FLAG)


.PHONY: clean

clean:
	rm $(OUT)