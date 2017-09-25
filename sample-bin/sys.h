#ifndef SYS_H
#define SYS_H

#define SYS_OUT 3
#define SYS_EXIT 10

int syscall(int r0, int r3, int r4, int r5, int r6, int r7, int r8, int r9) {
    int ret = 0;
    __asm__ __volatile__ (
            "add %%r3, %%r0, %1\n\t"
            "add %%r4, %%r0, %2\n\t"
            "add %%r5, %%r0, %3\n\t"
            "add %%r6, %%r0, %4\n\t"
            "add %%r7, %%r0, %5\n\t"
            "add %%r8, %%r0, %6\n\t"
            "add %%r9, %%r0, %7\n\t"
            "add %%r0, %%r0, %8\n\t"
            "sc\n\t"
            "add %0, %%r0, %%r3\n\t"
            : "=r"(ret)
            : "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7), "r"(r8), "r"(r9), "r"(r0)
            : "%r0", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9"
           );
}
int out(int o) {
    return syscall(SYS_OUT, o, 0, 0, 0, 0, 0, 0);
}
int exit(int code) {
    return syscall(SYS_EXIT, code, 0, 0, 0, 0, 0, 0);
}

#endif
