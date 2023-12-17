#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cpu.h"
#include "debug.h"

uint16_t memory_size = 0xFFFF;
bool debug = 0;
void read_test(unsigned char* memory, char* filename, uint16_t addr);

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s [--debug] filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc == 3) {
        if ((strcmp(argv[1], "--debug") != 0 && strcmp(argv[1], "-d") != 0)) {
            fprintf(stderr, "Usage: %s [--debug] filename\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        debug = 1;
    }

    Cpu cpu;
    unsigned char* rom = malloc(memory_size);
    cpu_init(&cpu, rom);

    read_test(cpu.memory, argv[argc - 1], 0x100);

    // Needed for syscall
    *(cpu.memory + 0x07) = 0xC9;

    if (debug) print_memory(&cpu, memory_size);

    while (1) {
        if (cpu.pc == 0x0000) return 0;
        if (debug) disassemble(&cpu);
        cpu_execute(&cpu);
        sys_call(&cpu);
        if (debug) register_state(&cpu);
    }

    free(rom);
    return 0;
}

void read_test(unsigned char* memory, char* filename, uint16_t addr) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    memory_size = (uint16_t)file_size;
    rewind(fp);

    const size_t test_size = fread(memory + addr, 1, memory_size, fp);
    if (test_size != file_size) {
        printf("Failed to read rom %s\n", filename);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
}
