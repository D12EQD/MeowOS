.section .text.sbi_section_start, "ax"

.globl _start
.globl mmain

_start:
    # init a stack
    la sp, __stack_top
    call mmain


