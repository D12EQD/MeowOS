OUTPUT_ARCH( "riscv" )

SECTIONS {
    . = 0x4000; /* 前4页空着，备用 */

    .text : {
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

    .data : {
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
    }

    PROVIDE(__user_end = .);
}
