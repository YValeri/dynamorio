How PADLOC works ?
============

At its core, **PADLOC** replaces the floating point instructions of an executable by their MCA counterpart. For that, it uses the DynamoRIO's API to decode the executable, insert new instructions and reencode the instructions to be executed.

___
# Instruction replacement

DynamoRIO allows us to get the different basic blocks of the program before they are executed. Upon receiving such basic block, we iterate over its instructions in order to find instrumentable instructions.
We instrument only the floating point operations and not the rest. 