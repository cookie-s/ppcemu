#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <sys/mman.h>
#include <elf.h>

// getconf PAGE_SIZE
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

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
    while(((mapping[i][0]>>32)&1) && mapping[i][0]&((1LL<<32)-1) != va_align) i++, i%=200;

    if(mapping[i][0]&((1LL<<32)-1) != va_align)
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
        while(((mapping[i][0]>>32)&1) && mapping[i][0]&((1LL<<32)-1) != ph->p_vaddr) i++, i%=200;
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
    while(((mapping[i][0]>>32)&1) && mapping[i][0]&((1LL<<32)-1) != STACK_TOP) i++, i%=200;
    mapping[i][0]  = STACK_TOP;
    mapping[i][0] |= 1LL<<32;
    mapping[i][1]  = (uint64_t)p;

    reg_pc = elf_hdr->e_entry;
    memset(reg_gr, 0, sizeof(reg_gr));
    reg_gr[1] = STACK_TOP + PAGE_SIZE - 0x20;

    printf("%p (%p): %08x\n", reg_pc, v2p(reg_pc), *(uint32_t*)v2p(reg_pc));

    return 0;
}
