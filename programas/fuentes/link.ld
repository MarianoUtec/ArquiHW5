/* Todo empieza en 0x0, igual que los binarios de CPUlator. */
ENTRY(_start)
SECTIONS {
    . = 0x0;
    .text   : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) *(.srodata*) }
    .data   : { *(.data*) *(.sdata*) }
    .bss    : { *(.bss*) *(.sbss*) *(COMMON) }
}
