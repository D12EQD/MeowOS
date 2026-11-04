.section .text._start, "ax"


.globl _start
_start:
    call user_main
    li a7, 8 # exit
    ecall
_loop:
    j _loop
