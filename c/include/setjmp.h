/* setjmp.h - jmp_buf holds BX, CX, DX, SI, DI, BP, SP and the return address */
#ifndef SETJMP_H
#define SETJMP_H

typedef unsigned jmp_buf[8];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif
