#include "cpu/exec.h"

make_EHelper(mov);

make_EHelper(operand_size);

make_EHelper(inv);
make_EHelper(nemu_trap);

// instr implements in pa2.part1
make_EHelper(push);
make_EHelper(pop);
make_EHelper(call);
make_EHelper(sub);
make_EHelper(xor);
make_EHelper(ret);


