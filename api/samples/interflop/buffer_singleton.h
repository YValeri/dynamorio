#ifndef SINGLETON_BUFFER_H
#define SINGLETON_BUFFER_H

#ifndef MAX_INSTR_OPND_COUNT

// Generally floating operations needs three operands (2src + 1 dst) and FMA needs 4 (3src + 1dst)
#define MAX_INSTR_OPND_COUNT 4
#endif

#ifndef MAX_OPND_SIZE_BYTES

// operand size is maximum 512 bits (AVX512 instructions) = 64 bytes 
#define MAX_OPND_SIZE_BYTES 64 
#endif


template <class T> class Buffer
{
    private : 

	    static T *unique_buffer;

    public : 

    bool getbuffer(float *buffer) {
        if(unique_buffer != nullptr) {
            buffer = (float*)unique_buffer;
            return true;
        }

        unique_buffer = (float*)malloc(INTERFLOP_BUFFER_SIZE);
        buffer = unique_buffer;
        return ((unique_buffer != nullptr) ? true : false);
    }

     bool getbuffer(double *buffer) {
        if(unique_buffer != nullptr) {
            buffer = (double*)unique_buffer;
            return true;
        }

        unique_buffer = (double*)malloc(INTERFLOP_BUFFER_SIZE);
        buffer = unique_buffer;
        return ((unique_buffer != nullptr) ? true : false);
    }

    bool free_buffer() {
        free(unique_buffer);
    }
};




#endif