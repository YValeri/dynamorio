#ifndef INTERFLOP_COMPUTE_HEADER
#define INTERFLOP_COMPUTE_HEADER

#include "./backend/interflop/backend.hxx"

//TODO remplacer par l'interface interflop
template <typename T>
void ifp_compute_add(T* operand_buffer, T* result_buffer)
{
    *result_buffer = Interflop::Op<T>::add(*(operand_buffer+1), *operand_buffer);
}

template <typename T>
void ifp_compute_sub(T* operand_buffer, T* result_buffer)
{
    *result_buffer = Interflop::Op<T>::sub(*(operand_buffer+1), *operand_buffer);
}

template <typename T>
void ifp_compute_mul(T* operand_buffer, T* result_buffer)
{
    *result_buffer = Interflop::Op<T>::mul(*(operand_buffer+1), *operand_buffer);
}

template <typename T>
void ifp_compute_div(T* operand_buffer, T* result_buffer)
{
    *result_buffer = Interflop::Op<T>::div(*(operand_buffer+1), *operand_buffer);
}

template <typename T>
void ifp_compute_add(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::add(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

template <typename T>
void ifp_compute_sub(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::sub(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

template <typename T>
void ifp_compute_mul(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::mul(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

template <typename T>
void ifp_compute_div(T* operand_buffer, T* result_buffer, unsigned char nbOfPrimitive)
{
    for(unsigned char i=0; i<nbOfPrimitive; ++i)
        *(result_buffer+i) = Interflop::Op<T>::div(*(operand_buffer+nbOfPrimitive+i), *operand_buffer+i);
}

#endif 