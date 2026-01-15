#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAXLINE 1024
#define MAX_SYMBOLS 1024

typedef enum {
    DB,
    DW,
    DD,
    DQ
} DATATYPE;

typedef enum {
    SEC_NONE,
    SEC_DATA,
    SEC_TEXT,
    SEC_BSS
} Section;

typedef enum {
    SYM_LABEL,
    SYM_VARIABLE,
    SYM_EXTERN,
    SYM_GLOBAL
} SymType;

typedef struct {
    char variable[100]; 
    DATATYPE size;
    char value[300];
} SECTIONENTRY;

typedef struct {
    char name[100];
    unsigned int address;
    SymType type;
    int defined;  // 1 if defined, 0 if extern/forward reference
    int size;     // Size in bytes for variables
    char section[20];  // Which section (.text, .data, .bss)
} SYMBOL;

SECTIONENTRY data[1024];
SECTIONENTRY TEXT[1024];
int datacount = 0;

// Add address counter
unsigned int current_address = 0;
int line_number = 1;

// Symbol table
SYMBOL symbol_table[MAX_SYMBOLS];
int symbol_count = 0;

// Function prototypes
int reg_code(const char *reg);
unsigned char mod_rm(int mod, const char *reg, const char *rm);
int is_register(const char *s);
int is_memory(const char *s);
int is_immediate(const char *s);
void check_operand(const char *op1, const char *op2, char *type);
int search_opcode(char *mnemonic, const char *type, char *out);
void Assembly_line(char *line);
void assembly_file(const char *filename);
void tolower_str(char *s);
void print_instruction(const char *original, unsigned char *machine, int len);
void reset_address_counter();
void add_symbol(const char *name, unsigned int address, SymType type, const char *section, int size, int defined);
SYMBOL* find_symbol(const char *name);
void print_symbol_table();
void process_data_line(char *line);
void process_bss_line(char *line);

void reset_address_counter() {
    current_address = 0;
    line_number = 1;
}

void add_symbol(const char *name, unsigned int address, SymType type, const char *section, int size, int defined) {
    if (symbol_count >= MAX_SYMBOLS) {
        fprintf(stderr, "Symbol table overflow!\n");
        return;
    }
    
    // Check if symbol already exists
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            // Update existing symbol if it's a definition
            if (defined) {
                symbol_table[i].address = address;
                symbol_table[i].defined = 1;
                symbol_table[i].type = type;
                strcpy(symbol_table[i].section, section);
                symbol_table[i].size = size;
            }
            return;
        }
    }
    
    // Add new symbol
    strcpy(symbol_table[symbol_count].name, name);
    symbol_table[symbol_count].address = address;
    symbol_table[symbol_count].type = type;
    symbol_table[symbol_count].defined = defined;
    strcpy(symbol_table[symbol_count].section, section);
    symbol_table[symbol_count].size = size;
    symbol_count++;
}

SYMBOL* find_symbol(const char *name) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return &symbol_table[i];
        }
    }
    return NULL;
}

void print_symbol_table() {
    printf("\n\nSymbol Table:\n");
    printf("Name                 Type     Value     Size  Section  Defined\n");
    printf("-------------------- -------- --------- ----- -------- -------\n");
    
    for (int i = 0; i < symbol_count; i++) {
        const char* type_str;
        switch(symbol_table[i].type) {
            case SYM_LABEL: type_str = "label"; break;
            case SYM_VARIABLE: type_str = "var"; break;
            case SYM_EXTERN: type_str = "extern"; break;
            case SYM_GLOBAL: type_str = "global"; break;
            default: type_str = "unknown";
        }
        
        printf("%-20s %-8s %08X %5d %-8s %s\n",
               symbol_table[i].name,
               type_str,
               symbol_table[i].address,
               symbol_table[i].size,
               symbol_table[i].section,
               symbol_table[i].defined ? "yes" : "no");
    }
}

void process_data_line(char *line) {
    // Parse data definition lines like:
    // var1: dd 100
    // var2 db 1,2,3
    // msg: db 'Hello',0
    
    char name[100] = "";
    char directive[10] = "";
    char value[300] = "";
    
    // Check for label
    char *colon = strchr(line, ':');
    if (colon) {
        int len = colon - line;
        strncpy(name, line, len);
        name[len] = '\0';
        
        // Remove leading/trailing spaces from name
        while (name[0] == ' ' || name[0] == '\t') {
            memmove(name, name + 1, strlen(name));
        }
        
        char *end = name + strlen(name) - 1;
        while (end > name && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        
        // Add to symbol table
        add_symbol(name, current_address, SYM_VARIABLE, ".data", 0, 1);
        
        // Move past colon
        line = colon + 1;
    }
    
    // Skip whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    // Parse directive
    sscanf(line, "%s", directive);
    char *rest = line + strlen(directive);
    
    // Calculate size based on directive
    int size = 0;
    DATATYPE dt;
    
    if (strcmp(directive, "db") == 0) {
        size = 1;
        dt = DB;
    } else if (strcmp(directive, "dw") == 0) {
        size = 2;
        dt = DW;
    } else if (strcmp(directive, "dd") == 0) {
        size = 4;
        dt = DD;
    } else if (strcmp(directive, "dq") == 0) {
        size = 8;
        dt = DQ;
    }
    
    // Parse the value(s)
    while (*rest == ' ' || *rest == '\t') rest++;
    
    // For now, just print the line
    printf("%4d                                      %s\n", line_number++, line);
    
    // Update current_address based on size
    // Note: This is simplified - actual size depends on the number of values
    current_address += size;
}

void process_bss_line(char *line) {
    // Parse BSS (uninitialized data) lines like:
    // buffer: resb 100
    // array: resd 10
    
    char name[100] = "";
    char directive[10] = "";
    int count = 0;
    
    // Check for label
    char *colon = strchr(line, ':');
    if (colon) {
        int len = colon - line;
        strncpy(name, line, len);
        name[len] = '\0';
        
        // Remove leading/trailing spaces
        while (name[0] == ' ' || name[0] == '\t') {
            memmove(name, name + 1, strlen(name));
        }
        
        char *end = name + strlen(name) - 1;
        while (end > name && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        
        line = colon + 1;
    }
    
    // Skip whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    // Parse directive and count
    sscanf(line, "%s %d", directive, &count);
    
    // Calculate size based on directive
    int size = 0;
    if (strcmp(directive, "resb") == 0) {
        size = count * 1;  // bytes
    } else if (strcmp(directive, "resw") == 0) {
        size = count * 2;  // words
    } else if (strcmp(directive, "resd") == 0) {
        size = count * 4;  // dwords
    } else if (strcmp(directive, "resq") == 0) {
        size = count * 8;  // qwords
    }
    
    // Add to symbol table if we have a name
    if (strlen(name) > 0) {
        add_symbol(name, current_address, SYM_VARIABLE, ".bss", size, 1);
    }
    
    // Print the line
    printf("%4d                                      %s\n", line_number++, line);
    
    // Update address
    current_address += size;
}

void print_instruction(const char *original, unsigned char *machine, int len) {
    // Print line number and address
    printf("%4d %08X ", line_number++, current_address);
    
    // Print machine code bytes
    for (int i = 0; i < len; i++) {
        printf("%02X", machine[i]);
    }
    
    // Add spaces for alignment (similar to NASM output)
    int bytes_printed = len * 2;
    int spaces_needed = 24 - bytes_printed;
    if (spaces_needed < 1) spaces_needed = 1;
    
    for (int i = 0; i < spaces_needed; i++) {
        printf(" ");
    }
    
    // Print the original assembly instruction
    printf("%s\n", original);
    
    // Update address counter
    current_address += len;
}

void tolower_str(char *s) {
    for (; *s; s++)
        *s = tolower((unsigned char)*s);
}

int reg_code(const char *reg)
{
    if (!reg) return -1;
    if (strcmp(reg, "eax") == 0) return 0;
    if (strcmp(reg, "ecx") == 0) return 1;
    if (strcmp(reg, "edx") == 0) return 2;
    if (strcmp(reg, "ebx") == 0) return 3;
    if (strcmp(reg, "esp") == 0) return 4;
    if (strcmp(reg, "ebp") == 0) return 5;
    if (strcmp(reg, "esi") == 0) return 6;
    if (strcmp(reg, "edi") == 0) return 7;
    return -1;  // not a register
}

unsigned char mod_rm(int mod, const char *reg, const char *rm)
{
    int regcode = reg_code(reg);
    int rmcode  = reg_code(rm);

    if (regcode < 0 || rmcode < 0) {
        printf("invalid register in mod_rm\n");
        return 0;
    }

    return (unsigned char)((mod << 6) | (regcode << 3) | rmcode);
}

int is_register(const char *s) {
    if (!s) return 0;
    if (s[0] == 'e' && (
                strstr(s, "ax") || strstr(s, "bx") || strstr(s, "cx") || strstr(s, "dx") ||
                strstr(s, "si") || strstr(s, "di") || strstr(s, "sp") || strstr(s, "bp")
             )) return 1;
    return 0;
}

int is_memory(const char *s) {
    if (!s) return 0;
    return (s[0] == '[');   // ex: [ebx+ecx*4+8]
}

int is_immediate(const char *s) {
    if (!s) return 0;
    if (isdigit((unsigned char)s[0])) return 1;
    if (s[0] == '-' && isdigit((unsigned char)s[1])) return 1;
    return 0;
}

void check_operand(const char *op1, const char *op2, char *type)
{
    if (is_register(op1) && is_register(op2))
        strcpy(type, "RR");
    else if (is_register(op1) && is_immediate(op2))
        strcpy(type, "RI");
    else if (is_memory(op1) && is_register(op2))
        strcpy(type, "MR");
    else if (is_register(op1) && is_memory(op2))
        strcpy(type, "RM");
    else
        strcpy(type, "??");
}

int search_opcode(char *mnemonic, const char *type, char *out)
{
    FILE *fp = fopen("opcode.csv", "r");
    if (!fp) return 0;

    char row[128], col1[32], col2[8], col3[32];
    for (int i = 0; mnemonic[i]; i++) 
        mnemonic[i] = toupper((unsigned char)mnemonic[i]);

    if (fgets(row, sizeof(row), fp) == NULL) { // skip header
        fclose(fp);
        return 0;
    }

    while (fgets(row, sizeof(row), fp)) {
        if (sscanf(row, "%31[^,],%7[^,],%31[^\n]", col1, col2, col3) != 3)
            continue;
            
        // Convert column 1 to uppercase
        for (int i = 0; col1[i]; i++) 
            col1[i] = toupper((unsigned char)col1[i]);

        if (strcmp(col1, mnemonic) == 0 && strcmp(col2, type) == 0) {
            strcpy(out, col3);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

void Assembly_line(char *line)
{
    char mnemonic[32], op1[32], op2[32];
    char original[MAXLINE];
    strcpy(original, line);  // KEEP the original before tokenizing

    // Tokenize the line into mnemonic, op1, op2
    mnemonic[0] = op1[0] = op2[0] = '\0';

    char line_copy[MAXLINE];
    strcpy(line_copy, line);
    
    char *tok = strtok(line_copy, " ,");
    if (!tok) return;

    strcpy(mnemonic, tok);

    tok = strtok(NULL, " ,");
    if (tok) {
        strcpy(op1, tok);
        tok = strtok(NULL, " ,");
        if (tok) strcpy(op2, tok);
    }

    // Determine operand type (RR, RI, MR, RM)
    char type[8];
    check_operand(op1, op2, type);

    // Find opcode from CSV
    char opcode_str[32];
    int len = 0;
    unsigned char machine[16];  // Increased size for complex addressing modes

    if (search_opcode(mnemonic, type, opcode_str)) {
        // Convert hex string to byte
        machine[len++] = (unsigned char)strtol(opcode_str, NULL, 16);
        
        if (strcmp(type, "RR") == 0) {
            // Both operands are registers
            machine[len++] = mod_rm(3, op1, op2);
        }
        else if (strcmp(type, "RI") == 0) {
            // Register to immediate
            int num = machine[0];
            num += reg_code(op1);
            machine[0] = (unsigned char)num;
            int imm = atoi(op2);
            
            // Handle different sizes of immediates
            if (imm >= -128 && imm <= 127) {
                machine[len++] = (unsigned char)(imm & 0xFF);
            } else if (imm >= -32768 && imm <= 32767) {
                machine[len++] = (unsigned char)(imm & 0xFF);
                machine[len++] = (unsigned char)((imm >> 8) & 0xFF);
            } else {
                machine[len++] = (unsigned char)(imm & 0xFF);
                machine[len++] = (unsigned char)((imm >> 8) & 0xFF);
                machine[len++] = (unsigned char)((imm >> 16) & 0xFF);
                machine[len++] = (unsigned char)((imm >> 24) & 0xFF);
            }
        }
        else if (strcmp(type, "RM") == 0) {
            // Register to Memory: reg , [mem]
            const char *reg = op1;      // destination register
            char mem[256];
            strcpy(mem, op2);           // source memory operand

            // Remove the brackets from memory operand
            if (mem[0] == '[') {
                memmove(mem, mem + 1, strlen(mem));
                if (mem[strlen(mem)-1] == ']')
                    mem[strlen(mem)-1] = '\0';
            }

            // Check if it's just a displacement (e.g., "[1234]")
            char *plus = strchr(mem, '+');
            char *star = strchr(mem, '*');

            if (!plus && !star) {
                // Case 1: [reg] or Case 5: [disp]
                if (is_register(mem)) {
                    // Case 1: [reg] - register indirect
                    if (strcmp(mem, "ebp") == 0 || strcmp(mem, "esp") == 0) {
                        // esp and ebp in SIB require special handling
                        machine[len++] = mod_rm(0, reg, "ebp");  // mod=00, rm=101 means [disp32]
                        machine[len++] = 0x00;  // SIB byte: scale=00, index=100(esp), base=101(ebp)
                        machine[len++] = 0x00;  // 32-bit displacement (0)
                        machine[len++] = 0x00;
                        machine[len++] = 0x00;
                        machine[len++] = 0x00;
                    } else {
                        machine[len++] = mod_rm(0, reg, mem);
                    }
                } else {
                    // Case 5: [disp] - direct memory address
                    machine[len++] = mod_rm(0, reg, "ebp");  // mod=00, rm=101 means [disp32]
                    int disp = atoi(mem);
                    machine[len++] = (unsigned char)(disp & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                }
            } else if (plus && !star) {
                // Case 2: [reg+disp]
                char *base_reg = mem;
                char *disp_str = plus + 1;
                *plus = '\0';  // split at '+'

                int disp = atoi(disp_str);
                int mod;

                if (disp == 0 && strcmp(base_reg, "ebp") != 0) {
                    mod = 0;  // [reg]
                } else if (disp >= -128 && disp <= 127) {
                    mod = 1;  // 8-bit displacement
                } else {
                    mod = 2;  // 32-bit displacement
                }

                if (strcmp(base_reg, "esp") == 0) {
                    // esp requires SIB byte
                    machine[len++] = mod_rm(mod, reg, "esp");
                    machine[len++] = 0x24;  // SIB: scale=00, index=100(esp), base=100(esp)

                    if (mod == 1) {
                        machine[len++] = (unsigned char)(disp & 0xFF);
                    } else if (mod == 2) {
                        machine[len++] = (unsigned char)(disp & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                    }
                } else {
                    machine[len++] = mod_rm(mod, reg, base_reg);

                    if (mod == 1) {
                        machine[len++] = (unsigned char)(disp & 0xFF);
                    } else if (mod == 2) {
                        machine[len++] = (unsigned char)(disp & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                    }
                }
            } else if (star) {
                // Case 3 & 4: [index+base*scale] or [index+base*scale+disp]
                // Format: [base+index*scale+disp] or [base+index*scale]

                char *base_reg = mem;
                char *index_part = strstr(mem, "+");
                char *scale_part = strstr(mem, "*");
                char *disp_part = NULL;

                // Find displacement if exists
                char *last_plus = strrchr(mem, '+');
                if (last_plus && last_plus > scale_part) {
                    disp_part = last_plus + 1;
                    *last_plus = '\0';
                }

                // Parse index*scale
                if (index_part) {
                    *index_part = '\0';
                    index_part++;
                }

                char *index_reg = index_part;
                int scale = 1;

                if (scale_part) {
                    *scale_part = '\0';
                    scale = atoi(scale_part + 1);
                }

                // Calculate scale encoding: 1=00, 2=01, 4=10, 8=11
                int scale_enc;
                switch (scale) {
                    case 1: scale_enc = 0; break;
                    case 2: scale_enc = 1; break;
                    case 4: scale_enc = 2; break;
                    case 8: scale_enc = 3; break;
                    default: scale_enc = 0; break;
                }

                // Calculate SIB byte: scale(2 bits) | index(3 bits) | base(3 bits)
                unsigned char sib = (unsigned char)((scale_enc << 6) | (reg_code(index_reg) << 3) | reg_code(base_reg));

                int mod;
                int disp = 0;

                if (disp_part) {
                    disp = atoi(disp_part);
                    if (disp == 0 && strcmp(base_reg, "ebp") != 0) {
                        mod = 0;
                    } else if (disp >= -128 && disp <= 127) {
                        mod = 1;
                    } else {
                        mod = 2;
                    }
                } else {
                    mod = 0;
                }

                machine[len++] = mod_rm(mod, reg, "esp");  // esp indicates SIB follows
                machine[len++] = sib;

                if (mod == 1) {
                    machine[len++] = (unsigned char)(disp & 0xFF);
                } else if (mod == 2) {
                    machine[len++] = (unsigned char)(disp & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                }
            }
        }
        else if (strcmp(type, "MR") == 0) {
            // Register to Memory: [mem] , reg
            // mov [destination_memory], source_register

            const char *mem = op1;      // destination memory operand  
            const char *reg = op2;      // source register

            // Remove the brackets from memory operand
            char mem_copy[256];
            strcpy(mem_copy, mem);

            if (mem_copy[0] == '[') {
                // Remove opening bracket
                memmove(mem_copy, mem_copy + 1, strlen(mem_copy));
                // Remove closing bracket
                if (mem_copy[strlen(mem_copy)-1] == ']')
                    mem_copy[strlen(mem_copy)-1] = '\0';
            }

            // Now parse mem_copy which contains the memory operand without brackets

            // Check for SIB addressing (contains '*')
            char *star = strchr(mem_copy, '*');

            if (star) {
                // Handle SIB addressing: [base+index*scale(+disp)]

                // Find the parts
                char *plus_before_star = NULL;
                char *plus_after_star = NULL;

                // Look for '+' before '*'
                for (char *p = mem_copy; p < star; p++) {
                    if (*p == '+') plus_before_star = p;
                }

                // Look for '+' after '*'
                for (char *p = star + 1; *p; p++) {
                    if (*p == '+') {
                        plus_after_star = p;
                        break;
                    }
                }

                // Parse components
                char base_reg[32] = "";
                char index_reg[32] = "";
                int scale = 1;
                int disp = 0;

                if (plus_before_star) {
                    // Has base register before index
                    *plus_before_star = '\0';
                    strcpy(base_reg, mem_copy);
                    strcpy(index_reg, plus_before_star + 1);
                } else {
                    // No base, only index*scale
                    strcpy(index_reg, mem_copy);
                }

                // Extract scale (remove everything after '*')
                char *scale_end = star;
                while (*scale_end && *scale_end != '+' && *scale_end != ']') {
                    scale_end++;
                }
                char scale_char = *scale_end;
                *scale_end = '\0';
                scale = atoi(star + 1);
                *scale_end = scale_char;

                // Extract displacement if exists
                if (plus_after_star) {
                    disp = atoi(plus_after_star + 1);
                }

                // Calculate scale encoding
                int scale_enc;
                switch (scale) {
                    case 1: scale_enc = 0; break;
                    case 2: scale_enc = 1; break;
                    case 4: scale_enc = 2; break;
                    case 8: scale_enc = 3; break;
                    default: scale_enc = 0; break;
                }

                // Calculate SIB byte
                int base_code, index_code;

                if (strlen(base_reg) > 0) {
                    base_code = reg_code(base_reg);
                    if (base_code < 0) base_code = 5; // EBP if invalid
                } else {
                    base_code = 5; // No base register
                }

                // Clean index register string (remove scale part)
                char *index_end = strchr(index_reg, '*');
                if (index_end) *index_end = '\0';

                index_code = reg_code(index_reg);
                if (index_code < 0) index_code = 4; // ESP if invalid

                unsigned char sib = (unsigned char)((scale_enc << 6) | (index_code << 3) | base_code);

                // Determine mod value
                int mod;
                if (strlen(base_reg) == 0) {
                    // No base register - always mod=00 with 32-bit displacement
                    mod = 0;
                } else if (disp == 0 && strcmp(base_reg, "ebp") != 0) {
                    mod = 0;
                } else if (disp >= -128 && disp <= 127) {
                    mod = 1;
                } else {
                    mod = 2;
                }

                // Add modrm byte (reg field has source register, rm=100 for SIB)
                machine[len++] = mod_rm(mod, reg, "esp");

                // Add SIB byte
                machine[len++] = sib;

                // Add displacement if needed
                if (mod == 1) {
                    machine[len++] = (unsigned char)(disp & 0xFF);
                } else if (mod == 2 || strlen(base_reg) == 0) {
                    // 32-bit displacement for large disp or no-base SIB
                    machine[len++] = (unsigned char)(disp & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                    machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                }

            } else {
                // NON-SIB addressing modes

                // Check for displacement: [reg+disp] or [disp]
                char *plus = strchr(mem_copy, '+');

                if (!plus) {
                    // Simple cases: [reg] or [disp]
                    if (is_register(mem_copy)) {
                        // [reg] - register indirect
                        machine[len++] = mod_rm(0, reg, mem_copy);
                    } else {
                        // [disp] - direct memory addressing
                        machine[len++] = mod_rm(0, reg, "ebp");
                        int disp = atoi(mem_copy);
                        machine[len++] = (unsigned char)(disp & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                    }
                } else {
                    // [reg+disp]
                    char base_reg[32];
                    strncpy(base_reg, mem_copy, plus - mem_copy);
                    base_reg[plus - mem_copy] = '\0';

                    int disp = atoi(plus + 1);
                    int mod;

                    if (disp == 0 && strcmp(base_reg, "ebp") != 0) {
                        mod = 0;
                    } else if (disp >= -128 && disp <= 127) {
                        mod = 1;
                    } else {
                        mod = 2;
                    }

                    machine[len++] = mod_rm(mod, reg, base_reg);

                    if (mod == 1) {
                        machine[len++] = (unsigned char)(disp & 0xFF);
                    } else if (mod == 2) {
                        machine[len++] = (unsigned char)(disp & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 8) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 16) & 0xFF);
                        machine[len++] = (unsigned char)((disp >> 24) & 0xFF);
                    }
                }
            }
        }

        // Use the new print function that includes address
        print_instruction(original, machine, len);

    } else {
        // For non-instruction lines (labels, directives, etc.)
        printf("%4d                                      %s\n", line_number++, original);
    }
}

// Read .asm file line-by-line and call Assembly_line()
void assembly_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    char line[MAXLINE];

    if (!fp) {
        perror("Cannot open .asm file");
        return;
    }
    
    // Reset counters for new file
    reset_address_counter();
    symbol_count = 0;  // Clear symbol table
    
    int current = SEC_NONE;
    char current_section[20] = "";
    
    while (fgets(line, sizeof(line), fp)) {
        // Trim newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) {
            printf("%4d\n", line_number++);
            continue;
        }
        
        // Convert to lowercase for processing (keep original for display)
        char original_line[MAXLINE];
        strcpy(original_line, line);
        tolower_str(line);

        if (strstr(line, ".data")) {
            current = SEC_DATA;
            strcpy(current_section, ".data");
            current_address = 0;  // Reset address for new section
            printf("%4d                                      %s\n", line_number++, original_line);
            continue;
        }
        else if (strstr(line, ".text")) {
            current = SEC_TEXT;
            strcpy(current_section, ".text");
            current_address = 0;  // Reset address for new section
            printf("%4d                                      %s\n", line_number++, original_line);
            continue;
        }
        else if (strstr(line, ".bss")) {
            current = SEC_BSS;
            strcpy(current_section, ".bss");
            current_address = 0;  // Reset address for new section
            printf("%4d                                      %s\n", line_number++, original_line);
            continue;
        }
        
        switch (current) {
            case SEC_DATA: { 
                process_data_line(line);
                break;
            }
            case SEC_BSS: { 
                process_bss_line(line);
                break;
            }
            case SEC_TEXT: {
                // Parse global directives
                if (strncmp(line, "global", 6) == 0) {
                    char *p = line + 6;
                    while (*p == ' ' || *p == '\t') p++;
                    
                    // Parse global symbols
                    char *tok = strtok(p, ",");
                    while (tok != NULL) {
                        // Clean up token
                        while (*tok == ' ' || *tok == '\t') tok++;
                        char *end = tok + strlen(tok) - 1;
                        while (end > tok && (*end == ' ' || *end == '\t' || *end == '\n'))
                            *end-- = '\0';
                        
                        // Add to symbol table as global (not yet defined)
                        add_symbol(tok, 0, SYM_GLOBAL, current_section, 0, 0);
                        
                        tok = strtok(NULL, ",");
                    }
                    printf("%4d                                      %s\n", line_number++, original_line);
                    continue;
                }

                // Parse extern directives
                if (strncmp(line, "extern", 6) == 0) {
                    char *p = line + 6;
                    while (*p == ' ' || *p == '\t') p++;
                    
                    // Parse extern symbols
                    char *tok = strtok(p, ",");
                    while (tok != NULL) {
                        // Clean up token
                        while (*tok == ' ' || *tok == '\t') tok++;
                        char *end = tok + strlen(tok) - 1;
                        while (end > tok && (*end == ' ' || *end == '\t' || *end == '\n'))
                            *end-- = '\0';
                        
                        // Add to symbol table as extern
                        add_symbol(tok, 0, SYM_EXTERN, "", 0, 0);
                        
                        tok = strtok(NULL, ",");
                    }
                    printf("%4d                                      %s\n", line_number++, original_line);
                    continue;
                }

                // Check if it's a label (ends with ':')
                char *colon = strchr(line, ':');
                if (colon != NULL) {
                    // Extract label name
                    char label_name[100];
                    strncpy(label_name, line, colon - line);
                    label_name[colon - line] = '\0';
                    
                    // Clean up label name
                    char *end = label_name + strlen(label_name) - 1;
                    while (end > label_name && (*end == ' ' || *end == '\t'))
                        *end-- = '\0';
                    
                    // Add to symbol table
                    add_symbol(label_name, current_address, SYM_LABEL, current_section, 0, 1);
                    
                    // Print label with its address
                    printf("%4d %08X                               %s\n", line_number++, current_address, original_line);
                    
                    // Check if there's code after the label
                    char *after_colon = colon + 1;
                    while (*after_colon == ' ' || *after_colon == '\t') after_colon++;
                    
                    if (strlen(after_colon) > 0) {
                        // Process instruction after label
                        Assembly_line(after_colon);
                    }
                } else {
                    // It's a regular instruction - process it
                    Assembly_line(line);
                }
                break;				      
            }
        }
    }

    fclose(fp);
}

int main() {
    printf("Line   Address   Machine Code             Assembly\n");
    printf("---- ---------- ------------------------ -------------------------\n");
    assembly_file("input1.asm");
    print_symbol_table();
    return 0;
}
