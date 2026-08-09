[BITS 16]
[ORG 0x7c00]

CODE_OFFSET equ 0x8
DATA_OFFSET equ 0x10

KERNEL_LOAD_SEG equ 0x1000
;KERNEL_START_ADDR equ 0x100000
KERNEL_START_ADDR equ 0x10000

MODE_INFO_ADDR equ 0x9000
VBE_STRUCT_ADDR equ 0x9200

start:
    cli; // Clear interrupts & disable them
    mov ax, 0x00
    mov ds, ax  ;Set data segment to 0
    mov es, ax  ;Set extra segment to 0
    mov ss, ax  ;Set stack segment to 0
    mov sp, 0x7c00 ;Set stack pointer to 0x7c00
    sti; // Enable interrupts

;load kernel
;mov bx, KERNEL_LOAD_SEG ;Set segment to load kernel
mov ax, KERNEL_LOAD_SEG
mov es, ax
mov bx, 0x0000

mov dh, 0x00 ;Set head to 0

mov dl, 0x80 ;Set drive to first hard disk

mov cl, 0x02 ;Set sector to 2 (first sector of kernel)

mov ch, 0x00 ;Set cylinder to 0

mov ah, 0x02 ;Set function to read sectors

mov al, 24 ;Number of sectors to read

int 0x13 ;Call BIOS interrupt to read sectors

jc disk_read_error ;Jump to error handler if carry flag is set

;mov ax, 0x0013
;int 0x10        ; Video BIOS

xor ax, ax
mov es, ax

; ---- VBE: get mode info ----
mov ax, 0x4F01
mov cx, 0x11B          ; 1280x1024, 24bpp
mov di, MODE_INFO_ADDR
int 0x10
cmp ax, 0x004F
jne vbe_error

; ---- VBE: set mode (bit 14 = use linear framebuffer) ----
mov ax, 0x4F02
mov bx, 0x11B | 0x4000
int 0x10
cmp ax, 0x004F
jne vbe_error

mov eax, [MODE_INFO_ADDR + 0x28]   ; physical framebuffer address
mov [VBE_STRUCT_ADDR + 0], eax

mov ax, [MODE_INFO_ADDR + 0x10]    ; pitch (bytes per scanline)
mov [VBE_STRUCT_ADDR + 4], ax

mov ax, [MODE_INFO_ADDR + 0x12]    ; width
mov [VBE_STRUCT_ADDR + 6], ax

mov ax, [MODE_INFO_ADDR + 0x14]    ; height
mov [VBE_STRUCT_ADDR + 8], ax

mov al, [MODE_INFO_ADDR + 0x19]    ; bpp
mov [VBE_STRUCT_ADDR + 10], al

jmp load_PM

load_PM:
    cli
    lgdt [gdt_descriptor] ;Load GDT
    mov eax, cr0
    or eax, 0x1 ;Set PE bit in CR0 to enable protected mode
    mov cr0, eax
    jmp CODE_OFFSET:Pmode_main ;Far jump to flush the instruction queue and enter protected mode

disk_read_error:
    hlt

vbe_error:
    hlt

;GDT Implementation
gdt_start:
    dd 0x0
    dd 0x0

    ;code segment descriptor
    dw 0xFFFF ;Limit low
    dw 0x0000 ;Base low
    db 0x00 ;Base middle
    db 10011010b ;Access byte
    db 11001111b ;Flags and limit high
    db 0x00 ;Base high

    ;data segment descriptor
    dw 0xFFFF ;Limit low
    dw 0x0000 ;Base low
    db 0x00 ;Base middle
    db 10010010b ;Access byte
    db 11001111b ;Flags and limit high
    db 0x00 ;Base high

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ;Size of GDT
    dd gdt_start ;Address of GDT

[BITS 32]
Pmode_main:
    mov ax, DATA_OFFSET ;Load data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov ebp, 0x9C00
    mov esp, ebp

    in al, 0x92
    or al, 2
    out 0x92, al

    jmp CODE_OFFSET:KERNEL_START_ADDR ;Jump to kernel entry point

times 510 - ($ - $$) db 0 ;Fill the rest of the boot sector with zeros


dw 0xAA55
