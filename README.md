Simple x86 Assembler – README
1. Overview
This project is a simple x86 assembler written in C. It reads an assembly
source file and converts supported instructions and data into machine code
displayed as a hexadecimal dump.
2. Supported Sections
The assembler supports .data, .bss, and .text sections. Each section is
processed separately with its own memory address tracking.
3. Data Section
The .data section supports db, dw, dd, and dq directives. It handles decimal,
hexadecimal, and string literals and converts them into byte representations.
4. BSS Section
The .bss section supports resb, resw, resd, and resq directives. Memory is
reserved but not initialized, and output is shown as ?? in hex dump.
5. Text Section
The .text section processes instructions such as mov, add, sub, push, pop,
and ret. Labels are stored in the symbol table with their addresses.
6. Opcode Handling
Instruction opcodes are loaded from an external opcode.csv file. Each opcode
is matched using mnemonic and operand type.
7. Operand Types
Operands are classified as register, immediate, memory, or combinations like
RR, RI, MR, RM, and NOOP.
8. Machine Code Generation
The assembler generates machine code using opcode bytes and ModR/M encoding
for register-based instructions.
9. Symbol Table
Labels, global symbols, and extern symbols are stored with address and section
information.
10. Output
The output is a NASM-like hex dump showing address, machine code, and source
instruction.
11. Compilation
Compile using: gcc cp2.c -o assembler
12. Execution
Run using: ./assembler input.asm
13. Limitations
The assembler supports only a limited instruction set and does not generate
object or executable files.


