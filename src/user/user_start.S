.section .text
.globl _start
.globl user_main
_start:
    call user_main
_loop:
    j _loop

.section .rodata
msg:
    .asciz "hello from user\n"
