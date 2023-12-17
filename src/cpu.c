#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

#define SET_SZP(cpu, val) \
    do { \
        cpu->zf = (val) == 0; \
        cpu->sf = (val) >> 7; \
        cpu->pf = parity(val); \
    } while(0)

uint8_t cpu_read_byte(Cpu* cpu) {
    return *(cpu->memory + cpu->pc++);
}

uint8_t cpu_read_next_byte(Cpu* cpu) {
    return *(cpu->memory + cpu->pc + 1);
}

uint16_t cpu_read_word(Cpu* cpu) {
    uint16_t word = *(cpu->memory + cpu->pc + 1) << 8 | *(cpu->memory + cpu->pc);
    cpu->pc += 2;
    return word;
}

uint8_t cpu_get_content_addr(Cpu* cpu, uint16_t addr) {
    return *(cpu->memory + addr);
}

static uint8_t parity(uint8_t val) {
    uint8_t num_bits = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (val & 0x1) num_bits++;
        val = val >> 1;
    }
    return (num_bits % 2 == 0);
}

static uint8_t carry_add(uint8_t a, uint8_t b, uint8_t cy) {
    uint16_t result = a + b + cy;
    return (result > UINT8_MAX);
}

static uint8_t carry_sub(uint8_t a, uint8_t b, uint8_t cy) {
    int16_t difference = a - b - cy;
    return (difference < 0);
}

static uint8_t get_content_addr_in_reg(Cpu* cpu, uint8_t rh, uint8_t rl) {
    return *(cpu->memory + ((rh << 8) | rl));
}

static void set_content_addr(Cpu* cpu, uint16_t addr, uint8_t content) {
    *(cpu->memory + addr) = content;
}

static void set_content_addr_in_reg(Cpu* cpu, uint8_t rh, uint8_t rl, uint8_t content) {
    *(cpu->memory + ((rh << 8) | rl)) = content;
}

static uint16_t get_reg_pair(uint8_t rh, uint8_t rl) {
    return ((rh << 8) | rl);
}

static void set_reg_pair(uint8_t* rh, uint8_t* rl, uint16_t word) {
    *rh = word >> 8;
    *rl = word & 0xFF;
}

static void NOP(void) {
    return;
}

static void pop(Cpu* cpu, uint8_t* rh, uint8_t* rl) {
    *rl = *(cpu->memory + cpu->sp);
    *rh = *(cpu->memory + cpu->sp + 1);
    cpu->sp += 2;
}

static void pop_psw(Cpu* cpu) {
    uint8_t content = cpu_get_content_addr(cpu, cpu->sp);
    cpu->cf = content & 0x1;  // 0001
    cpu->pf = content & 0x4;  // 0100
    cpu->af = content & 0x10; // 10000
    cpu->zf = content & 0x40; // 1000000
    cpu->sf = content & 0x80; // 10000000
    cpu->a = cpu_get_content_addr(cpu, cpu->sp + 1);
    cpu->sp += 2;
}

static void push(Cpu* cpu, uint8_t rh, uint8_t rl) {
    set_content_addr(cpu, cpu->sp - 1, rh);
    set_content_addr(cpu, cpu->sp - 2, rl);
    cpu->sp -= 2;
}

static void push_psw(Cpu* cpu) {
    set_content_addr(cpu, cpu->sp - 1, cpu->a);
    uint8_t content = 0x00;
    content |= cpu->cf;
    content |= (uint8_t)(1 << 1); // Not needed but w/e
    content |= (cpu->pf << 2);
    content |= (uint8_t)(0 << 3);
    content |= (cpu->af << 4);
    content |= (uint8_t)(0 << 5);
    content |= (cpu->zf << 6);
    content |= (cpu->sf << 7);
    set_content_addr(cpu, cpu->sp - 2, content);
    cpu->sp -= 2;
}

static void RRC(Cpu* cpu) {
    uint8_t bit0 = cpu->a & 0x1;
    cpu->a = cpu->a >> 1;
    if (bit0) cpu->a |= (bit0 << 7); // Set bit7 while keeping all other bits
    else cpu->a &= (0xFF >> 1); // Clear bit7 while keeping all other bits
    cpu->cf = bit0;
}

static void RAR(Cpu* cpu) {
    uint8_t bit0 = cpu->a & 0x1;
    cpu->a = cpu->a >> 1;
    if (cpu->cf) cpu->a |= (cpu->cf << 7); // Set bit7 while keeping all other bits
    else cpu->a &= (0xFF >> 1); // Clear bit7 while keeping all other bits
    cpu->cf = bit0;
}

static void RLC(Cpu* cpu) {
    uint8_t bit7 = cpu->a >> 7;
    cpu->a = cpu->a << 1;
    cpu->a = (cpu->a) | bit7;
    cpu->cf = bit7;
}

static void RAL(Cpu* cpu) {
    uint8_t bit7 = cpu->a >> 7;
    cpu->a = cpu->a << 1;
    cpu->a = (cpu->a) | cpu->cf;
    cpu->cf = bit7;
}

static void CALL(Cpu* cpu, uint16_t word) {
    *(cpu->memory + (uint16_t)(cpu->sp - 1)) = cpu->pc >> 8;
    *(cpu->memory + (uint16_t)(cpu->sp - 2)) = cpu->pc & 0xFF;
    cpu->sp -= 2;
    cpu->pc = word;
}

static void RET(Cpu* cpu) {
    cpu->pc = cpu_get_content_addr(cpu, cpu->sp);
    cpu->pc |= cpu_get_content_addr(cpu, cpu->sp + 1) << 8;
    cpu->sp += 2;
}

static void DCR(Cpu* cpu, uint8_t* reg) {
    *reg -= 1;
    SET_SZP(cpu, *reg);
    cpu->af = !((*reg & 0xF) == 0xF); // always a carry except when value before decrementing is 0x0
}

static void DCX(uint8_t* rh, uint8_t* rl) {
    uint16_t reg_pair = get_reg_pair(*rh, *rl);
    uint16_t result = reg_pair -= 1;
    set_reg_pair(rh, rl, result);
}

static void INR(Cpu* cpu, uint8_t* reg) {
    *reg += 1;
    SET_SZP(cpu, *reg);
    cpu->af = (*reg & 0xF) == 0; // carry when value before incrementing is 0xF
}

static void INX(uint8_t* rh, uint8_t* rl) {
    uint16_t reg_pair = get_reg_pair(*rh, *rl);
    uint16_t result = reg_pair += 1;
    set_reg_pair(rh, rl, result);
}

static void XRA(Cpu* cpu, uint8_t reg) {
    cpu->a ^= reg;
    SET_SZP(cpu, cpu->a);
    cpu->af = 0;
    cpu->cf = 0;
}

static void ADD(Cpu* cpu, uint8_t reg) {
    uint8_t prev = cpu->a;
    cpu->a += reg;
    SET_SZP(cpu, cpu->a);
    cpu->af = ((prev & 0xF) + (reg & 0xF)) & 0x10;
    cpu->cf = carry_add(prev, reg, 0);
}

static void SUB(Cpu* cpu, uint8_t reg) {
    uint8_t prev = cpu->a;
    cpu->a -= reg;
    SET_SZP(cpu, cpu->a);
    cpu->af = ((prev & 0xF) + ((~reg & 0xF) + 1)) & 0x10; // a - b = a + (-b)
    cpu->cf = carry_sub(prev, reg, 0);
}

static void ADC(Cpu* cpu, uint8_t reg) {
    uint8_t prev = cpu->a;
    cpu->a = prev + reg + cpu->cf;
    SET_SZP(cpu, cpu->a);
    cpu->af = ((prev & 0xF) + (reg & 0xF) + cpu->cf) & 0x10;
    cpu->cf = carry_add(prev, reg, cpu->cf);
}

static void SBB(Cpu* cpu, uint8_t reg) {
    uint8_t prev = cpu->a;
    cpu->a = prev - reg - cpu->cf;
    SET_SZP(cpu, cpu->a);
    cpu->af = ((prev & 0xF) + (~reg & 0xF) + !cpu->cf) & 0x10;
    cpu->cf = carry_sub(prev, reg, cpu->cf);
}

static void ANA(Cpu* cpu, uint8_t reg) {
    uint8_t prev = cpu->a;
    cpu->a &= reg;
    SET_SZP(cpu, cpu->a);
    cpu->af = ((prev | reg) & 0x08) != 0; // https://www.quora.com/What-is-the-auxiliary-carry-set-when-ANA-R-instruction-is-executed-in-an-8085-CPU
    cpu->cf = 0;
}

static void ORA(Cpu* cpu, uint8_t reg) {
    cpu->a |= reg;
    SET_SZP(cpu, cpu->a);
    cpu->cf = 0;
    cpu->af = 0;
}

static void CMP(Cpu* cpu, uint8_t reg) {
    uint8_t result = cpu->a - reg;
    SET_SZP(cpu, result);
    cpu->cf = carry_sub(cpu->a, reg, 0);
    cpu->af = ((cpu->a & 0xF) + ((~reg & 0xF) + 1)) & 0x10;
}

static void LDAX(Cpu* cpu, uint8_t rh, uint8_t rl) {
    uint16_t reg_pair = get_reg_pair(rh, rl);
    cpu->a = cpu_get_content_addr(cpu, reg_pair);
}

static void STAX(Cpu* cpu, uint8_t rh, uint8_t rl) {
    uint16_t reg_pair = get_reg_pair(rh, rl);
    set_content_addr(cpu, reg_pair, cpu->a);
}

static void DAD(Cpu* cpu, uint8_t rh, uint8_t rl) {
    uint16_t hl = get_reg_pair(cpu->h, cpu->l);
    uint16_t old_hl = hl;
    uint16_t reg_pair = get_reg_pair(rh, rl);
    hl += reg_pair;
    set_reg_pair(&cpu->h, &cpu->l, hl);
    cpu->cf = (uint32_t)(old_hl + reg_pair > UINT16_MAX);
}

static void DAA(Cpu* cpu) {
    uint8_t cy = 0;
    uint8_t least_sig_4_bits = cpu->a & 0xF;
    uint8_t correction = 0;
    if (least_sig_4_bits > 9 || cpu->af) {
        correction |= 0x06;
    }
    uint8_t most_sig_4_bits = cpu->a >> 4;
    if (most_sig_4_bits > 9 || cpu->cf || (most_sig_4_bits >= 9 && least_sig_4_bits > 9)) {
        correction |= 0x60;
        cy = 1;
    }
    ADD(cpu, correction);
    cpu->cf = cy;
}

static void OUT() {
    return;
}

static void IN() {
    return;
}

void cpu_execute(Cpu* cpu) {
    uint8_t opcode = cpu_read_byte(cpu);
    switch (opcode) {
        case 0x00: NOP(); break;
                   // LXI
        case 0x01: set_reg_pair(&cpu->b, &cpu->c, cpu_read_word(cpu)); break;
        case 0x02: STAX(cpu, cpu->b, cpu->c); break;
        case 0x03: INX(&cpu->b, &cpu->c); break;
        case 0x04: INR(cpu, &cpu->b); break;
        case 0x05: DCR(cpu, &cpu->b); break;
                   // MVI
        case 0x06: cpu->b = cpu_read_byte(cpu); break;
        case 0x07: RLC(cpu); break;
        case 0x08: NOP(); break;
        case 0x09: DAD(cpu, cpu->b, cpu->c); break;
        case 0x0A: LDAX(cpu, cpu->b, cpu->c); break;
        case 0x0B: DCX(&cpu->b, &cpu->c); break;
        case 0x0C: INR(cpu, &cpu->c); break;
        case 0x0D: DCR(cpu, &cpu->c); break;
                   // MVI
        case 0x0E: cpu->c = cpu_read_byte(cpu); break;
        case 0x0F: RRC(cpu); break;
        case 0x10: NOP(); break;
                   // LXI
        case 0x11: set_reg_pair(&cpu->d, &cpu->e, cpu_read_word(cpu)); break;
        case 0x12: STAX(cpu, cpu->d, cpu->e); break;
        case 0x13: INX(&cpu->d, &cpu->e); break;
        case 0x14: INR(cpu, &cpu->d); break;
        case 0x15: DCR(cpu, &cpu->d); break;
                   // MVI
        case 0x16: cpu->d = cpu_read_byte(cpu); break;
        case 0x17: RAL(cpu); break;
        case 0x18: NOP(); break;
        case 0x19: DAD(cpu, cpu->d, cpu->e); break;
        case 0x1A: LDAX(cpu, cpu->d, cpu->e); break;
        case 0x1B: DCX(&cpu->d, &cpu->e); break;
        case 0x1C: INR(cpu, &cpu->e); break;
        case 0x1D: DCR(cpu, &cpu->e); break;
                   // MVI
        case 0x1E: cpu->e = cpu_read_byte(cpu); break;
        case 0x1F: RAR(cpu); break;
        case 0x20: NOP(); break;
                   // LXI
        case 0x21: set_reg_pair(&cpu->h, &cpu->l, cpu_read_word(cpu)); break;
                  // SHLD
        case 0x22: {
            uint16_t word = cpu_read_word(cpu);
            set_content_addr(cpu, word, cpu->l);
            set_content_addr(cpu, word + 1, cpu->h);
            break;
        }
        case 0x23: INX(&cpu->h, &cpu->l); break;
        case 0x24: INR(cpu, &cpu->h); break;
        case 0x25: DCR(cpu, &cpu->h); break;
                   // MVI
        case 0x26: cpu->h = cpu_read_byte(cpu); break;
        case 0x27: DAA(cpu); break;
        case 0x28: NOP(); break;
        case 0x29: DAD(cpu, cpu->h, cpu->l); break;
                   // LHLD
        case 0x2A: {
            uint16_t word = cpu_read_word(cpu);
            cpu->l = cpu_get_content_addr(cpu, word);
            cpu->h = cpu_get_content_addr(cpu, word + 1);
            break;
        }
        case 0x2B: DCX(&cpu->h, &cpu->l); break;
        case 0x2C: INR(cpu, &cpu->l); break;
        case 0x2D: DCR(cpu, &cpu->l); break;
                   // MVI
        case 0x2E: cpu->l = cpu_read_byte(cpu); break;
                   // CMA
        case 0x2F: cpu->a = ~cpu->a; break;
        case 0x30: NOP(); break;
                   // LXI
        case 0x31: cpu->sp = cpu_read_word(cpu); break;
                   // STA
        case 0x32: set_content_addr(cpu, cpu_read_word(cpu), cpu->a); break;
                   // INX
        case 0x33: cpu->sp += 1; break;
        case 0x34: INR(cpu, (cpu->memory + get_reg_pair(cpu->h, cpu->l))); break;
        case 0x35: DCR(cpu, (cpu->memory + get_reg_pair(cpu->h, cpu->l))); break;
                   // MVI
        case 0x36: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu_read_byte(cpu)); break;
                   // STC
        case 0x37: cpu->cf = 1; break;
        case 0x38: NOP(); break;
        case 0x39: DAD(cpu, cpu->sp >> 8, cpu->sp & 0xFF); break;
                   // LDA
        case 0x3A: {
            uint16_t word = cpu_read_word(cpu);
            cpu->a = cpu_get_content_addr(cpu, word);
            break;
        }
                   // DCX
        case 0x3B: cpu->sp -= 1; break;
        case 0x3C: INR(cpu, &cpu->a); break;
        case 0x3D: DCR(cpu, &cpu->a); break;
                   // MVI
        case 0x3E: cpu->a = cpu_read_byte(cpu); break;
                   // CMC
        // MOV Instructions
        case 0x3F: cpu->cf = !cpu->cf; break;
        case 0x40: cpu->b = cpu->b; break;
        case 0x41: cpu->b = cpu->c; break;
        case 0x42: cpu->b = cpu->d; break;
        case 0x43: cpu->b = cpu->e; break;
        case 0x44: cpu->b = cpu->h; break;
        case 0x45: cpu->b = cpu->l; break;
        case 0x46: cpu->b = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x47: cpu->b = cpu->a; break;
        case 0x48: cpu->c = cpu->b; break;
        case 0x49: cpu->c = cpu->c; break;
        case 0x4A: cpu->c = cpu->d; break;
        case 0x4B: cpu->c = cpu->e; break;
        case 0x4C: cpu->c = cpu->h; break;
        case 0x4D: cpu->c = cpu->l; break;
        case 0x4E: cpu->c = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x4F: cpu->c = cpu->a; break;
        case 0x50: cpu->d = cpu->b; break;
        case 0x51: cpu->d = cpu->c; break;
        case 0x52: cpu->d = cpu->d; break;
        case 0x53: cpu->d = cpu->e; break;
        case 0x54: cpu->d = cpu->h; break;
        case 0x55: cpu->d = cpu->l; break;
        case 0x56: cpu->d = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x57: cpu->d = cpu->a; break;
        case 0x58: cpu->e = cpu->b; break;
        case 0x59: cpu->e = cpu->c; break;
        case 0x5A: cpu->e = cpu->d; break;
        case 0x5B: cpu->e = cpu->e; break;
        case 0x5C: cpu->e = cpu->h; break;
        case 0x5D: cpu->e = cpu->l; break;
        case 0x5E: cpu->e = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x5F: cpu->e = cpu->a; break;
        case 0x60: cpu->h = cpu->b; break;
        case 0x61: cpu->h = cpu->c; break;
        case 0x62: cpu->h = cpu->d; break;
        case 0x63: cpu->h = cpu->e; break;
        case 0x64: cpu->h = cpu->h; break;
        case 0x65: cpu->h = cpu->l; break;
        case 0x66: cpu->h = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x67: cpu->h = cpu->a; break;
        case 0x68: cpu->l = cpu->b; break;
        case 0x69: cpu->l = cpu->c; break;
        case 0x6a: cpu->l = cpu->d; break;
        case 0x6b: cpu->l = cpu->e; break;
        case 0x6c: cpu->l = cpu->h; break;
        case 0x6d: cpu->l = cpu->l; break;
        case 0x6e: cpu->l = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x6f: cpu->l = cpu->a; break;
        case 0x70: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->b); break;
        case 0x71: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->c); break;
        case 0x72: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->d); break;
        case 0x73: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->e); break;
        case 0x74: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->h); break;
        case 0x75: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->l); break;
        case 0x76: NOP(); break;
        case 0x77: set_content_addr_in_reg(cpu, cpu->h, cpu->l, cpu->a); break;
        case 0x78: cpu->a = cpu->b; break;
        case 0x79: cpu->a = cpu->c; break;
        case 0x7a: cpu->a = cpu->d; break;
        case 0x7b: cpu->a = cpu->e; break;
        case 0x7c: cpu->a = cpu->h; break;
        case 0x7d: cpu->a = cpu->l; break;
        case 0x7e: cpu->a = get_content_addr_in_reg(cpu, cpu->h, cpu->l); break;
        case 0x7f: cpu->a = cpu->a; break;
        case 0x80: ADD(cpu, cpu->b); break;
        case 0x81: ADD(cpu, cpu->c); break;
        case 0x82: ADD(cpu, cpu->d); break;
        case 0x83: ADD(cpu, cpu->e); break;
        case 0x84: ADD(cpu, cpu->h); break;
        case 0x85: ADD(cpu, cpu->l); break;
        case 0x86: ADD(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0x87: ADD(cpu, cpu->a); break;
        case 0x88: ADC(cpu, cpu->b); break;
        case 0x89: ADC(cpu, cpu->c); break;
        case 0x8a: ADC(cpu, cpu->d); break;
        case 0x8b: ADC(cpu, cpu->e); break;
        case 0x8c: ADC(cpu, cpu->h); break;
        case 0x8d: ADC(cpu, cpu->l); break;
        case 0x8e: ADC(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0x8f: ADC(cpu, cpu->a); break;
        case 0x90: SUB(cpu, cpu->b); break;
        case 0x91: SUB(cpu, cpu->c); break;
        case 0x92: SUB(cpu, cpu->d); break;
        case 0x93: SUB(cpu, cpu->e); break;
        case 0x94: SUB(cpu, cpu->h); break;
        case 0x95: SUB(cpu, cpu->l); break;
        case 0x96: SUB(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0x97: SUB(cpu, cpu->a); break;
        case 0x98: SBB(cpu, cpu->b); break;
        case 0x99: SBB(cpu, cpu->c); break;
        case 0x9a: SBB(cpu, cpu->d); break;
        case 0x9b: SBB(cpu, cpu->e); break;
        case 0x9c: SBB(cpu, cpu->h); break;
        case 0x9d: SBB(cpu, cpu->l); break;
        case 0x9e: SBB(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0x9f: SBB(cpu, cpu->a); break;
        case 0xa0: ANA(cpu, cpu->b); break;
        case 0xa1: ANA(cpu, cpu->c); break;
        case 0xa2: ANA(cpu, cpu->d); break;
        case 0xa3: ANA(cpu, cpu->e); break;
        case 0xa4: ANA(cpu, cpu->h); break;
        case 0xa5: ANA(cpu, cpu->l); break;
        case 0xa6: ANA(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0xa7: ANA(cpu, cpu->a); break;
        case 0xa8: XRA(cpu, cpu->b); break;
        case 0xa9: XRA(cpu, cpu->c); break;
        case 0xaa: XRA(cpu, cpu->d); break;
        case 0xab: XRA(cpu, cpu->e); break;
        case 0xac: XRA(cpu, cpu->h); break;
        case 0xad: XRA(cpu, cpu->l); break;
        case 0xae: XRA(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0xaf: XRA(cpu, cpu->a); break;
        case 0xb0: ORA(cpu, cpu->b); break;
        case 0xb1: ORA(cpu, cpu->c); break;
        case 0xb2: ORA(cpu, cpu->d); break;
        case 0xb3: ORA(cpu, cpu->e); break;
        case 0xb4: ORA(cpu, cpu->h); break;
        case 0xb5: ORA(cpu, cpu->l); break;
        case 0xb6: ORA(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0xb7: ORA(cpu, cpu->a); break;
        case 0xb8: CMP(cpu, cpu->b); break;
        case 0xb9: CMP(cpu, cpu->c); break;
        case 0xba: CMP(cpu, cpu->d); break;
        case 0xbb: CMP(cpu, cpu->e); break;
        case 0xbc: CMP(cpu, cpu->h); break;
        case 0xbd: CMP(cpu, cpu->l); break;
        case 0xbe: CMP(cpu, get_content_addr_in_reg(cpu, cpu->h, cpu->l)); break;
        case 0xbf: CMP(cpu, cpu->a); break;
                   // RNZ
        case 0xc0: if (!cpu->zf) RET(cpu); break;
        case 0xc1: pop(cpu, &cpu->b, &cpu->c); break;
                   // JNZ
        case 0xc2: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->zf) cpu->pc = word;
            break;
        }
                   // JMP
        case 0xc3: cpu->pc = cpu_read_word(cpu); break;
                   // CNZ
        case 0xc4: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->zf) CALL(cpu, word);
            break;
        }
        case 0xc5: push(cpu, cpu->b, cpu->c); break;
                   // ADI
        case 0xc6: {
            uint8_t prev = cpu->a;
            uint8_t byte = cpu_read_byte(cpu);
            cpu->a += byte;
            SET_SZP(cpu, cpu->a);
            cpu->af = ((prev & 0xF) + (byte & 0xF)) & 0x10;
            cpu->cf = carry_add(prev, byte, 0);
            break;
        }
                   // RST 0
        case 0xc7: CALL(cpu, 0x00); break;
                   // RZ
        case 0xc8: if (cpu->zf) RET(cpu); break;
        case 0xc9: RET(cpu); break;
                   // JZ
        case 0xca: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->zf) cpu->pc = word;
            break;
        }
        case 0xcb: NOP(); break;
                   // CZ
        case 0xcc: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->zf) CALL(cpu, word);
            break;
        }
        case 0xcd: CALL(cpu, cpu_read_word(cpu)); break;
                   // ACI
        case 0xce: {
            uint8_t prev = cpu->a;
            uint8_t byte = cpu_read_byte(cpu);
            cpu->a = prev + byte + cpu->cf;
            SET_SZP(cpu, cpu->a);
            cpu->af = ((prev & 0xF) + (byte & 0xF) + cpu->cf) & 0x10;
            cpu->cf = carry_add(prev, byte, cpu->cf);
            break;
        }
                   // RST 1
        case 0xcf: CALL(cpu, 0x08); break;
                   // RNC
        case 0xd0: if (!cpu->cf) RET(cpu); break;
        case 0xd1: pop(cpu, &cpu->d, &cpu->e); break;
                   // JNC
        case 0xd2: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->cf) cpu->pc = word;
            break;
        }
        case 0xd3: OUT(); break;
                   // CNC
        case 0xd4: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->cf) CALL(cpu, word);
            break;
        }
        case 0xd5: push(cpu, cpu->d, cpu->e); break;
                   // SUI
        case 0xd6: {
            uint8_t prev = cpu->a;
            uint8_t byte = cpu_read_byte(cpu);
            cpu->a -= byte;
            SET_SZP(cpu, cpu->a);
            cpu->af = ((prev & 0xF) + ((~byte & 0xF) + 1)) & 0x10;
            cpu->cf = carry_sub(prev, byte, 0);
            break;
        }
                   // RST 2
        case 0xd7: CALL(cpu, 0x10); break;
                   // RC
        case 0xd8: if (cpu->cf) RET(cpu); break;
        case 0xd9: NOP(); break;
                   // JC
        case 0xda: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->cf) cpu->pc = word;
            break;
        }
        case 0xdb: IN(); break;
                   // CC
        case 0xdc: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->cf) CALL(cpu, word);
            break;
        }
        case 0xdd: NOP(); break;
                   // SBI
        case 0xde: {
            uint8_t prev = cpu->a;
            uint8_t byte = cpu_read_byte(cpu);
            cpu->a = prev - byte - cpu->cf;
            SET_SZP(cpu, cpu->a);
            cpu->af = ((prev & 0xF) + (~byte & 0xF) + !cpu->cf) & 0x10;
            cpu->cf = carry_sub(prev, byte, cpu->cf);
            break;
        }
                   // RST 3
        case 0xdf: CALL(cpu, 0x18); break;
                   // RPO
        case 0xe0: if (!cpu->pf) RET(cpu); break;
        case 0xe1: pop(cpu, &cpu->h, &cpu->l); break;
                   // JPO
        case 0xe2: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->pf) cpu->pc = word;
            break;
        }
                   // XTHL
        case 0xe3: {
            uint8_t old_h = cpu->h;
            uint8_t old_l = cpu->l;
            cpu->l = cpu_get_content_addr(cpu, cpu->sp);
            cpu->h = cpu_get_content_addr(cpu, cpu->sp + 1);
            set_content_addr(cpu, cpu->sp, old_l);
            set_content_addr(cpu, cpu->sp + 1, old_h);
            break;
        }
                   // CPO
        case 0xe4: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->pf) CALL(cpu, word);
            break;
        }
        case 0xe5: push(cpu, cpu->h, cpu->l); break;
                   // ANI
        case 0xe6: {
            uint8_t prev = cpu->a;
            uint8_t byte = cpu_read_byte(cpu);
            cpu->a &= byte;
            SET_SZP(cpu, cpu->a);
            cpu->af = ((prev | byte) & 0x08) != 0;
            cpu->cf = 0;
            break;
        }
                   // RST 4
        case 0xe7: CALL(cpu, 0x20); break;
                   // RPE
        case 0xe8: if (cpu->pf) RET(cpu); break;
                   // PCHL
        case 0xe9: cpu->pc = get_reg_pair(cpu->h, cpu->l); break;
                   // JPE
        case 0xea: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->pf) cpu->pc = word;
            break;
        }
                   // XCHG
        case 0xeb: {
            uint8_t d_temp = cpu->d;
            uint8_t e_temp = cpu->e;
            cpu->d = cpu->h;
            cpu->e = cpu->l;
            cpu->h = d_temp;
            cpu->l = e_temp;
            break;
        }
                   // CPE
        case 0xec: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->pf) CALL(cpu, word);
            break;
        }
        case 0xed: NOP(); break;
                   // XRI
        case 0xee: {
            cpu->a ^= cpu_read_byte(cpu);
            SET_SZP(cpu, cpu->a);
            cpu->cf = 0;
            cpu->af = 0;
            break;
        }
                   // RST 5
        case 0xef: CALL(cpu, 0x28); break;
                   // RP
        case 0xf0: if (!cpu->sf) RET(cpu); break;
        case 0xf1: pop_psw(cpu); break;
                   // JP
        case 0xf2: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->sf) cpu->pc = word;
            break;
        }
                   // DI
        case 0xf3: cpu->interrupt = 0; break;
                   // CP
        case 0xf4: {
            uint16_t word = cpu_read_word(cpu);
            if (!cpu->sf) CALL(cpu, word);
            break;
        }
        case 0xf5: push_psw(cpu); break;
                   // ORI
        case 0xf6: {
            cpu->a |= cpu_read_byte(cpu);
            SET_SZP(cpu, cpu->a);
            cpu->cf = 0;
            cpu->af = 0;
            break;
        }
                   // RST 6
        case 0xf7: CALL(cpu, 0x30); break;
                   // RM
        case 0xf8: if (cpu->sf) RET(cpu); break;
                   // SPHL
        case 0xf9: cpu->sp = get_reg_pair(cpu->h, cpu->l); break;
                   // JM
        case 0xfa: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->sf) cpu->pc = word;
            break;
        }
                   // EI
        case 0xfb: cpu->interrupt = 1; break;
                   // CM
        case 0xfc: {
            uint16_t word = cpu_read_word(cpu);
            if (cpu->sf) CALL(cpu, word);
            break;
        }
        case 0xfd: NOP(); break;
                   // CPI
        case 0xfe: {
            uint8_t byte = cpu_read_byte(cpu);
            uint8_t result = cpu->a - byte;
            SET_SZP(cpu, result);
            cpu->cf = carry_sub(cpu->a, byte, 0);
            cpu->af = ((cpu->a & 0xF) + ((~byte & 0xF) + 1)) & 0x10;
            break;
        }
                   // RST 7
        case 0xff: CALL(cpu, 0x38); break;
        default: break;
    }
}

void cpu_init(Cpu* cpu, unsigned char* rom) {
    cpu->a = 0;
    cpu->b = 0;
    cpu->c = 0;
    cpu->d = 0;
    cpu->e = 0;
    cpu->h = 0;
    cpu->l = 0;
    cpu->sp = 0;
    cpu->pc = 0x100;
    cpu->memory = rom;

    cpu->zf = 0;
    cpu->sf = 0;
    cpu->pf = 0;
    cpu->cf = 0;
    cpu->af = 0;

    cpu->interrupt = 0;
}
