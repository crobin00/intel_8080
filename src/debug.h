#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include "cpu.h"

uint16_t disassemble(Cpu* cpu);
void register_state(Cpu* cpu);
void print_memory(Cpu* cpu, uint16_t memory_size);
void sys_call(Cpu* cpu);

#endif
