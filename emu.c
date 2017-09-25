#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <sys/mman.h>
#include <elf.h>

#include "error.h"

// getconf PAGE_SIZE
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define LT_SHIFT 31
#define GT_SHIFT 30
#define EQ_SHIFT 29
#define SO_SHIFT 28

#define STACK_TOP 0xFC000000

char binary[10000];

// [0] virtual addr, [1] lower physical addr.
// [0] upper 32bit LSB indicates validity.
// rwx is (now) always permitted.
// only 200 mapping.
uint64_t mapping[200][2];

uint32_t reg_gr[32];
uint32_t reg_lr;
uint32_t reg_cr;
uint32_t reg_pc;


uint64_t hash(uint64_t in) {
    uint64_t out = in;
    out ^= out << 3;
    out ^= out >> 10;
    out ^= out << 6;
    out ^= out << 20;
    out ^= out >> 14;
    out ^= out << 17;
    return out;
}

void* v2p(uint32_t va) {
    uint32_t va_align = (va >> PAGE_SHIFT) << PAGE_SHIFT;
    int i = hash(va_align) % 200;
    while(((mapping[i][0]>>32)&1) && (mapping[i][0]&((1LL<<32)-1)) != va_align) i++, i%=200;

    if((mapping[i][0]&((1LL<<32)-1)) != va_align)
        return NULL;

    return (void*)mapping[i][1] + (va - va_align);
}

int chk_support_elf() {
    Elf32_Ehdr *elf_hdr = (Elf32_Ehdr*)binary;

    if(strncmp(ELFMAG, elf_hdr->e_ident, 4)) {
        fputs("not elf~~~\n", stderr);
        return 1;
    }

    if(elf_hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        fputs("not 32bit~~~\n", stderr);
        return 1;
    }

    if(elf_hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        fputs("not le~~~\n", stderr);
        return 1;
    }

    if(elf_hdr->e_machine != EM_PPC) {
        fputs("not ppc~~~\n", stderr);
        return 1;
    }

    return 0;
}

uint32_t chgendian(uint32_t in) {
    uint32_t out = 0;
    out |= ((in >> 0) & 0xFF) << 24;
    out |= ((in >> 8) & 0xFF) << 16;
    out |= ((in >>16) & 0xFF) << 8;
    out |= ((in >>24) & 0xFF) << 0;
    return out;
}

int step() {
    uint32_t instr = *(uint32_t*)v2p(reg_pc);
    uint8_t opcode = (instr >> (31-5)) & ((1<<6)-1);
    printf("%d\n", opcode);

    // I-Form
    uint32_t i_li = (instr >> 2) & ((1<<24)-1);
    uint8_t  i_aa = (instr >> 1) & 1;
    uint8_t  i_lk = (instr >> 0) & 1;

    // B-Form
    uint8_t  b_bo = (instr >> 20) & ((1<<5)-1);
    uint8_t  b_bi = (instr >> 16) & ((1<<5)-1);
    uint16_t b_bd = (instr >>  2) & ((1<<14)-1);
    uint8_t  b_aa = (instr >> 1) & 1;
    uint8_t  b_lk = (instr >> 0) & 1;

    // D-Form
    uint8_t d_D = (instr >> 20) & ((1<<5)-1); // or S
    uint8_t d_A = (instr >> 16) & ((1<<5)-1);
    int8_t d_d = (instr >>  0) & ((1<<16)-1);
    uint8_t d_crfD = d_D >> 2;
    uint8_t d_L = d_D & 1;

    uint32_t EA;

    reg_pc += 4;
    switch(opcode) {
        case 14: // addi
            reg_gr[d_D] = d_A ? reg_gr[d_A] + d_d : d_d;
            break;
        case 19: // TODO
            break;
        case 31: // X-Form
            {
                uint8_t x_D = (instr >> 20) & ((1<<5)-1); // or S
                uint8_t x_A = (instr >> 16) & ((1<<5)-1);
                uint8_t x_B = (instr >> 12) & ((1<<5)-1);
                uint8_t x_crfD = d_D >> 2;
                uint8_t x_crfS = (instr >> 16) & ((1<<3)-1);
                uint16_t x_XO = (instr >> 1) & ((1<<10)-1);
                uint8_t x_Rc = d_D & 1;
                switch(x_XO) {
                    case 444: // or
                        reg_gr[x_A] = reg_gr[x_D] | reg_gr[x_B];
                        if(x_Rc) {
                            reg_cr = 0;
                            if(reg_gr[x_A] == 0)
                                reg_cr |= 1<<EQ_SHIFT;
                            // FIXME What does means LT, GT, SO ?
                        }
                        break;
                    default:
                        return E_ILLEGAL_INSTRUCTION;
                }
            }
            break;
        case 32: //lwz
            EA = d_A ? reg_gr[d_A] + d_d : d_d;
            reg_gr[d_D] = *(uint32_t*)v2p(EA);
            break;
        case 36: //stw
            EA = d_d + (d_A == 0 ? 0 : reg_gr[d_A]);
            *(uint32_t*)v2p(EA) = reg_gr[d_D];
            break;
        case 37: //stwu
            if(d_A == 0) return E_ILLEGAL_INSTRUCTION;
            EA = reg_gr[d_A] + d_d;
            *(uint32_t*)v2p(EA) = reg_gr[d_D];
            reg_gr[d_A] = EA;
            break;
        case 44: //sth
            EA = d_d + (d_A == 0 ? 0 : reg_gr[d_A]);
            *(uint16_t*)v2p(EA) = reg_gr[d_D] & 0xFFFF;
            break;
        case 45: //sthu
            if(d_A == 0) return E_ILLEGAL_INSTRUCTION;
            EA = reg_gr[d_A] + d_d;
            *(uint16_t*)v2p(EA) = reg_gr[d_D] & 0xFFFF;
            reg_gr[d_A] = EA;
            break;
        default:
            return E_ILLEGAL_INSTRUCTION;
    }

    return 0;
}

int main(int argc, char **argv) {
    if(argc < 2) {
        fputs("binary~~~\n", stderr);
        return 1;
    }

    FILE *fpbin = fopen(argv[1], "r");
    if(!fpbin) {
        perror("fopen binary");
        return 1;
    }

    fread(binary, 10000, 1, fpbin);

    if(chk_support_elf()) return 1;

    Elf32_Ehdr *elf_hdr = (Elf32_Ehdr*)binary;

    // load sections
    Elf32_Phdr *ph = (Elf32_Phdr*)(binary + elf_hdr->e_phoff);
    for(int i=0; i<elf_hdr->e_phnum; i++, ph+=elf_hdr->e_phentsize) {
        if(ph->p_type != PT_LOAD) continue;
        void *p = mmap(NULL, ph->p_memsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        if(!p) {
            perror("alloc page fail");
            return 1;
        }
        memcpy(p, binary + ph->p_offset, ph->p_filesz);

        int i = hash(ph->p_vaddr) % 200;
        while(((mapping[i][0]>>32)&1) && (mapping[i][0]&((1LL<<32)-1)) != ph->p_vaddr) i++, i%=200;
        mapping[i][0]  = ph->p_vaddr;
        mapping[i][0] |= 1LL<<32;
        mapping[i][1]  = (uint64_t)p;
    }

    // alloc stack
    void *p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if(!p) {
        perror("alloc stack page fail");
        return 1;
    }

    int i = hash(STACK_TOP) % 200;
    while(((mapping[i][0]>>32)&1) && (mapping[i][0]&((1LL<<32)-1)) != STACK_TOP) i++, i%=200;
    mapping[i][0]  = STACK_TOP;
    mapping[i][0] |= 1LL<<32;
    mapping[i][1]  = (uint64_t)p;

    reg_pc = elf_hdr->e_entry;
    memset(reg_gr, 0, sizeof(reg_gr));
    reg_gr[1] = STACK_TOP + PAGE_SIZE - 0x100;
    reg_gr[2] = 0; // FIXME What is TOC???

    while(!step());
    reg_pc -= 4;
    printf("%p (%p): %08x\n", reg_pc, v2p(reg_pc), *(uint32_t*)v2p(reg_pc));

    return 0;
}
