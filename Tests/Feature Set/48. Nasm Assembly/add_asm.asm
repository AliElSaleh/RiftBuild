; Windows x64 calling convention: first two integer args in ecx, edx.
bits 64
default rel

section .text
global add_numbers

add_numbers:
    lea eax, [rcx + rdx]
    ret
