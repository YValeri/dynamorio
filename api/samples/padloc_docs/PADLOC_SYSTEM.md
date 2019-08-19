How PADLOC works ?
============

At its core, **PADLOC** replaces the floating point instructions of an executable by their MCA counterpart. For that, it uses the DynamoRIO's API to decode the executable, insert new instructions and reencode the instructions to be executed.

___
# Instruction replacement

DynamoRIO allows us to get the different basic blocks of the program before they are executed. Upon receiving such basic block, we iterate over its instructions in order to find instrumentable instructions. We instrument only the floating point operations and not the rest.

Currently, the operations we instrument are :
- Addition (ADD)
- Substraction (SUB)
- Division (DIV)
- Multiplication (MUL)
- Fused Multiply Add (FMA)
- Fused Multiply Sub (FMS)
- Fused Negated Multiply Add (FNMA)
- Fused Negated Multiply Sub (FNMS)

We currently support the FMA3, SSE, AVX and AVX2 instruction sets, but not AVX-512 or FMA4.

When we encounter the first instrumentable instruction, we prepare the processor state to call a backend function. This backend function signature is defined by the Interflop interface and needs to replace the instruction it instruments. For ease of use, we call a function that only works with pointers as a middleware in order to call the backend accordingly. This is useful for packed instructions.

## Registers Safekeeping

To not interfere with the behavior of the program outside of the MCA disturbance, we need to make sure that every register that isn't affected by the instrumented instruction stays unaffected, independently of the backend used. For that, we save all the general purpose registers (GPR) and the arithmetic flags in a buffer in memory. This buffer is a thread local storage (TLS) to prevent multi-thread corruption. We do the same for all the SIMD registers.

Note that saving the arithmetic flags is important as the backends have a tendency to alter them. The instructions we instrument don't normally affect the arithmetic flags and thus we need to make sure they stay untouched. It's not uncommon to see a comparison with a GPR followed by a floating point operations before the result of that comparison is used.

## Setting up the result TLS

In order for the middleware to have a place to put the result, we have a TLS that contains an adress. All the instructions we instrument have a SIMD register as destination. Thus the result TLS point towards the location in memory 

## Preparation of the calling convention

 We use the LEA instruction to setup the calling convention registers as addresses to the corresponding parameters.

When the instrumented instruction's operand is a SIMD register, we load the address of the saved register 