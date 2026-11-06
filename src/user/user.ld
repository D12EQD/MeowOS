OUTPUT_ARCH( "riscv" )
ENTRY(_start)

SECTIONS {
    /* 0x0000-0x0FFF 保留：NULL 陷阱页 */
    . = 0x1000;

    .data : {
        LONG(0x4d454f57) /* 魔数 见 src/kernel/mm/def.h */
        . = ALIGN(16);
        *(.sdata .sdata.*)
        . = ALIGN(16);
        *(.data .data.*)
    }

    .bss : {
        . = ALIGN(16);
        *(.sbss .sbss.*)
        . = ALIGN(16);
        *(.bss .bss.*)
        *(COMMON)
    }

    ASSERT(. <= 0x4000, "user static data exceeds 12KB")

    . = 0x4000;

    .text : {
        KEEP(*(.text._start));
        *(.text .text.*)
    }

    .rodata : {
        . = ALIGN(16);
        *(.srodata .srodata.*) 
        . = ALIGN(16);
        *(.rodata .rodata.*)
    }

    .eh_frame : {
        *(.eh_frame)
        *(.eh_frame.*)
    }

    . = ALIGN(0x1000);
    PROVIDE(__user_end = .);
}
