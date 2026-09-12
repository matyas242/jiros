ENTRY(_start)
OUTPUT_FORMAT(binary)
SECTIONS
{
    . = 0x10000;
    .text : ALIGN(4096)
    {
        *(.text)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata)
    }

    .data : ALIGN(4096)
    {
        *(.data)
    }

    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss)
    }

    .kernel_end ALIGN(512) :
    {
        LONG(0xDEADBEEF)
        . = ALIGN(512);
    }
}
