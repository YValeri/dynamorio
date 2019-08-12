CC=g++
SOURCE=$(wildcard *.cpp) 
OUT=$(SOURCE:%.cpp=%.out)

all: $(OUT)

call.out: call.cpp
	$(CC) -o call.out call.cpp -march=native -fopenmp -O3

commutativity.out: commutativity.cpp
	$(CC) -o commutativity.out commutativity.cpp -O3

divtest.out: divtest.cpp
	$(CC) -o divtest.out divtest.cpp -O3 -masm=intel

matmul_blas.out: matmul_blas.cpp
	$(CC) -o matmul_blas.out matmul_blas.cpp -lopenblas -O3 -march=native

rump.out: rump.cpp
	$(CC) -o rump.out rump.cpp -O3

test_instructions_sse.out: test_instructions.h test_instructions_sse.cpp
	$(CC) -o test_instructions_sse.out test_instructions_sse.cpp -msse -O3

test_instructions_avx.out: test_instructions.h test_instructions_avx.cpp
	$(CC) -o test_instructions_avx.out test_instructions_avx.cpp -mavx -O3

test_instructions_fma.out: test_instructions.h test_instructions_fma.cpp
	$(CC) -o test_instructions_fma.out test_instructions_fma.cpp -mfma -O3

test_fma_order.out: test_fma_order.cpp
	$(CC) -o test_fma_order.out test_fma_order.cpp -masm=intel -O3

test_parallel.out: test_parallel.cpp
	$(CC) -o test_parallel.out test_parallel.cpp -fopenmp -mavx -O3


.PHONY: clean

clean:
	rm $(OUT)