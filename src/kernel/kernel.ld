ENTRY(boot) /*  ELF 的入口点是符号 boot */

SECTIONS {
    /* OpenSBI 之后内核加载到 0x80200000 */
    . = 0x80200000;
    
    .text :{
        PROVIDE(__start_text_section = .);
        /* boot 放最前面 KEEP 强调强制保留 */
        KEEP(*(.text.boot));
        *(.text .text.*); 
        . = ALIGN(0x1000);
        
        /* 蹦床代码 */
        __trampoline_section_start = . ;

        *(.trampoline_section) ;
        . = ALIGN(0x1000);

        __trampoline_section_end = . ;

        ASSERT(__trampoline_section_end - __trampoline_section_start == 0x1000, "error: trampoline larger than one page");
        PROVIDE(__end_text_section = .);
    }
    
    __start_other_section = .;
    .rodata : ALIGN(16) {
        *(.rodata .rodata.*);
    }

    .data : ALIGN(16) {
        *(.data .data.*);
    }
    
    /* 静态变量和只读数据区域，一般默认需要清空 */
    .bss : ALIGN(16) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }
    
    /* stack area */
    .stack : ALIGN(16) {
        __stack_bottom = .;
        . += 128 * 1024; /* 128KB */
        __stack_top = .;
    }
    . = ALIGN(0x1000);
    __end_other_section = .;

    
    .free_ram (NOLOAD) : ALIGN(4096) {
        __free_ram = .;
        . += 64 * 1024 * 1024; /* 64MB */
        __free_ram_end = .;
    }
}
