[BITS 16]
[ORG 0x7c00]

CODE_OFFSET equ 0x8
DATA_OFFSET equ 0x10

KERNEL_LOAD_SEG equ 0x1000
;KERNEL_START_ADDR equ 0x100000
KERNEL_START_ADDR equ 0x10000

MODE_INFO_ADDR equ 0x9000
VBE_STRUCT_ADDR equ 0x9200

KERNEL_MAGIC equ 0xDEADBEEF

start:
    cli; // Clear interrupts & disable them
    mov ax, 0x00
    mov ds, ax  ;Set data segment to 0
    mov es, ax  ;Set extra segment to 0
    mov ss, ax  ;Set stack segment to 0
    mov sp, 0x7c00 ;Set stack pointer to 0x7c00
    sti; // Enable interrupts

    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx

    mov cl, 2
    xor ch, ch
    xor dh, dh
    mov dl, 0x80

    xor si, si

.count_loop:
    mov ah, 0x02
    mov al, 1
    int 0x13
    jc .counting_done

    mov di, bx
    mov eax, [es:di]
    cmp eax, KERNEL_MAGIC
    je .counting_done

    inc si
    inc cl
    add bh, 2
    jmp .count_loop

.counting_done:
    mov [kernel_sector_count], si

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

mov al, [kernel_sector_count] ;Number of sectors to read

int 0x13 ;Call BIOS interrupt to read sectors

jc disk_read_error ;Jump to error handler if carry flag is set

;mov ax, 0x0013
;int 0x10        ; Video BIOS

xor ax, ax
mov es, ax

; VBE: get mode info
mov ax, 0x4F01
mov cx, 0x11B          ; 1280x1024, 24bpp
mov di, MODE_INFO_ADDR
int 0x10
cmp ax, 0x004F
jne vbe_error

; VBE: set mode bit 14 = use linear framebuffer
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

kernel_sector_count:
    dw 0

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

    ;jmp CODE_OFFSET:KERNEL_START_ADDR ;Jump to kernel entry point

; check if CPUID is supported
EFLAGS_ID equ 1 << 21

checkCPUID:
    pushfd
    pop eax

    ; The original value should be saved for comparison and restoration later
    mov ecx, eax
    xor eax, EFLAGS_ID

    ; storing the eflags and then retrieving it again will show whether or not
    ; the bit could successfully be flipped
    push eax                    ; save to eflags
    popfd
    pushfd                      ; restore from eflags
    pop eax

    ; Restore EFLAGS to its original value
    push ecx
    popfd

    ; if the bit in eax was successfully flipped (eax != ecx), CPUID is supported.
    xor eax, ecx
    jnz check_if_longmode_supported

notSupported:
    mov ax, 0
    ret

; check longmode support
CPUID_EXTENSIONS equ 0x80000000 ; returns the maximum extended requests for cpuid
CPUID_EXT_FEATURES equ 0x80000001 ; returns flags containing long mode support among other things

CPUID_EDX_EXT_FEAT_LM equ 1 << 29   ; if this is set, the CPU supports long mode

NoLongMode:
    hlt

check_if_longmode_supported:
    mov eax, CPUID_EXTENSIONS
    cpuid
    cmp eax, CPUID_EXT_FEATURES
    jb NoLongMode                ; if the CPU can't report long mode support, then it likely
                                  ; doesn't support it

    mov eax, CPUID_EXT_FEATURES
    cpuid
    test edx, CPUID_EDX_EXT_FEAT_LM
    jz NoLongMode

    jmp disablePaging32

; disable paging
CR0_PAGING equ 1 << 31

disablePaging32:
    mov eax, cr0
    and eax, ~CR0_PAGING
    mov cr0, eax
    jmp tables

PML4T_ADDR equ 0x1000
PDPT_ADDR  equ 0x2000

PT_ADDR_MASK equ 0xffffffffff000
PT_PRESENT   equ 1 << 0
PT_READABLE  equ 1 << 1
PT_PAGE_SIZE equ 1 << 7

SIZEOF_PAGE_TABLE equ 4096

; clear tables
tables:
    mov edi, PML4T_ADDR
    mov cr3, edi       ; cr3 lets the CPU know where the page tables are

    xor eax, eax
    mov ecx, (SIZEOF_PAGE_TABLE * 2) / 4   ; clear PML4T + PDPT (2 pages, in dwords)
    rep stosd
    mov edi, cr3       ; reset edi back to the beginning of the page table

    ; PML4[0] -> PDPT
    mov DWORD [edi], PDPT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE

    mov edi, PDPT_ADDR
    mov DWORD [edi + 0],  0x00000000 | PT_PRESENT | PT_READABLE | PT_PAGE_SIZE
    mov DWORD [edi + 8],  0x40000000 | PT_PRESENT | PT_READABLE | PT_PAGE_SIZE
    mov DWORD [edi + 16], 0x80000000 | PT_PRESENT | PT_READABLE | PT_PAGE_SIZE
    mov DWORD [edi + 24], 0xC0000000 | PT_PRESENT | PT_READABLE | PT_PAGE_SIZE

    jmp PEA_enable

CR4_PAE_ENABLE equ 1 << 5

PEA_enable:
    mov eax, cr4
    or eax, CR4_PAE_ENABLE
    mov cr4, eax
    jmp PML5

CPUID_GET_FEATURES equ 7
CPUID_FEATURE_PML5 equ 1 << 16

CR4_LA57 equ 1 << 12

five_level_paging:
    mov eax, cr4
    or eax, CR4_LA57
    mov cr4, eax

PML5:
    mov eax, CPUID_GET_FEATURES
    xor ecx, ecx
    cpuid
    test ecx, CPUID_FEATURE_PML5
    jnz five_level_paging


EFER_MSR equ 0xC0000080
EFER_LM_ENABLE equ 1 << 8

CR0_PM_ENABLE equ 1 << 0
CR0_PG_ENABLE equ 1 << 31

compatibility_mode:
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LM_ENABLE
    wrmsr

    ; enable paging and protected mode
    mov eax, cr0
    or eax, CR0_PG_ENABLE | CR0_PM_ENABLE   ; ensuring that PM is set will allow for jumping
                                            ; from real mode to compatibility mode directly
    mov cr0, eax

    lgdt [GDT.Pointer]
    jmp GDT.Code:Realm64

; Access bits
PRESENT        equ 1 << 7
NOT_SYS        equ 1 << 4
EXEC           equ 1 << 3
DC             equ 1 << 2
RW             equ 1 << 1
ACCESSED       equ 1 << 0

; Flags bits
GRAN_4K       equ 1 << 7
SZ_32         equ 1 << 6
LONG_MODE     equ 1 << 5

GDT:
    .Null: equ $ - GDT
        dq 0
    .Code: equ $ - GDT
        .Code.limit_lo: dw 0xffff
        .Code.base_lo: dw 0
        .Code.base_mid: db 0
        .Code.access: db PRESENT | NOT_SYS | EXEC | RW
        .Code.flags: db GRAN_4K | LONG_MODE | 0xF   ; Flags & Limit (high, bits 16-19)
        .Code.base_hi: db 0
    .Data: equ $ - GDT
        .Data.limit_lo: dw 0xffff
        .Data.base_lo: dw 0
        .Data.base_mid: db 0
        .Data.access: db PRESENT | NOT_SYS | RW
        .Data.Flags: db GRAN_4K | SZ_32 | 0xF       ; Flags & Limit (high, bits 16-19)
        .Data.base_hi: db 0
    .Pointer:
        dw $ - GDT - 1
        dq GDT

[BITS 64]
Realm64:
    cli
    mov ax, GDT.Data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp KERNEL_START_ADDR

    hlt


times 510 - ($ - $$) db 0 ;Fill the rest of the boot sector with zeros

dw 0xAA55