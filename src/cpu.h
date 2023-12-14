#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    uint8_t* memory;

    bool sf : 1, zf : 1, af : 1, pf : 1, cf : 1;

    bool interrupt;
} Cpu;

void cpu_init(Cpu*, unsigned char*);
uint8_t cpu_get_content_addr(Cpu* cpu, uint16_t addr);
uint8_t cpu_read_byte(Cpu*);
uint8_t cpu_read_next_byte(Cpu*);
uint16_t cpu_read_word(Cpu*);
void cpu_execute(Cpu*);
#endif
