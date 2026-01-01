#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include<ctype.h>
#define MAXLINE 1024


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

// skip(fp, word)
// Return 1 = found the word, 0 = reached EOF without finding it.
int skip(FILE *fp, const char *word) {
	char buffer[MAXLINE];

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (strstr(buffer, word)) {
			// Stop at first matching line (word found)
			return 1;
		}
	}
	return 0; // EOF reached without matching
}
int is_register(char *s) {
	if (!s) return 0;
	if (s[0]=='e' && (
				strstr(s,"ax") || strstr(s,"bx") || strstr(s,"cx") || strstr(s,"dx") ||
				strstr(s,"si") || strstr(s,"di") || strstr(s,"sp") || strstr(s,"bp")
			 )) return 1;
	return 0;
}

int is_memory(char *s) {
	if (!s) return 0;
	return (s[0] == '[');   // ex: [ebx+ecx*4+8]
}

int is_immediate(char *s) {
	if (!s) return 0;
	if (isdigit(s[0])) return 1;
	if (s[0]=='-' && isdigit(s[1])) return 1;
	return 0;
}

void check_operand(char *op1, char *op2, char *type)
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

int search_opcode(char *mnemonic, char *type, char *out)
{
	FILE *fp = fopen("opcode.csv", "r");
	if (!fp) return 0;

	char row[128], col1[32], col2[8], col3[32];
	for (int i = 0; col1[i]; i++) col1[i] = toupper(col1[i]);
	for (int i = 0; mnemonic[i]; i++) mnemonic[i] = toupper(mnemonic[i]);


	fgets(row, sizeof(row), fp); // skip header

	while (fgets(row, sizeof(row), fp)) {
		sscanf(row, "%[^,],%[^,],%[^\n]", col1, col2, col3);

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


	//Tokenize the line into mnemonic, op1, op2
	mnemonic[0] = op1[0] = op2[0] = 0;

	char *tok = strtok(line, " ,");
	if (!tok) return;

	strcpy(mnemonic, tok);

	tok = strtok(NULL, " ,");
	if (tok) strcpy(op1, tok);

	tok = strtok(NULL, " ,");
	if (tok) strcpy(op2, tok);

	//Determine operand type (RR, RI, MR, RM)
	char type[8];
	check_operand(op1, op2, type);

	//Find opcode from CSV
	unsigned char opcode[32];
	int len=0;
	unsigned char machine[8];
	if (search_opcode(mnemonic, type, opcode)) {
		//printf("Opcode: %s %s : %s\n", mnemonic, type, opcode);
		//Inside the search validate the instructions
		machine[len++] = (unsigned char) strtol(opcode, NULL, 16);
		if(strcmp(type,"RR")==0){
			//both operand is reg (calculate mod R/M)
			machine[len++]=mod_rm(3,op1,op2);
		}
		if(strcmp(type,"RI")==0){
			//operand is Reg to imm
			int num=machine[0];
			num+=reg_code(op1);
			machine[0]=num;
			int imm=atoi(op2);
			machine[len++]=imm;
		}

		if(strcmp(type,"RM")==0){
			//it should  handle
			//[reg]
			//[reg+disp]
			//[index+base*scale+disp]
			//[index+base*scale]if(strcmp(type,"MR")==0){
			// Memory to register: reg , [mem]
			// This is for instructions like: mov reg, [mem]
			// So op1 = register, op2 = memory operand

			char *reg = op1;      // destination register
			char *mem = op2;      // source memory operand

			// Remove the brackets from memory operand
			mem++;  // skip '['
			mem[strlen(mem)-1] = '\0';  // remove ']'

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
					machine[len++] = disp & 0xFF;
					machine[len++] = (disp >> 8) & 0xFF;
					machine[len++] = (disp >> 16) & 0xFF;
					machine[len++] = (disp >> 24) & 0xFF;
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
						machine[len++] = disp & 0xFF;
					} else if (mod == 2) {
						machine[len++] = disp & 0xFF;
						machine[len++] = (disp >> 8) & 0xFF;
						machine[len++] = (disp >> 16) & 0xFF;
						machine[len++] = (disp >> 24) & 0xFF;
					}
				} else {
					machine[len++] = mod_rm(mod, reg, base_reg);

					if (mod == 1) {
						machine[len++] = disp & 0xFF;
					} else if (mod == 2) {
						machine[len++] = disp & 0xFF;
						machine[len++] = (disp >> 8) & 0xFF;
						machine[len++] = (disp >> 16) & 0xFF;
						machine[len++] = (disp >> 24) & 0xFF;
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
				unsigned char sib = (scale_enc << 6) | (reg_code(index_reg) << 3) | reg_code(base_reg);

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
					machine[len++] = disp & 0xFF;
				} else if (mod == 2) {
					machine[len++] = disp & 0xFF;
					machine[len++] = (disp >> 8) & 0xFF;
					machine[len++] = (disp >> 16) & 0xFF;
					machine[len++] = (disp >> 24) & 0xFF;
				}
			}
		}


		/*		if(strcmp(type,"MR")==0){
		//oprand is  register to memory
		}
		*/
		if(strcmp(type,"MR")==0){
			// Register to Memory: [mem] , reg
			// mov [destination_memory], source_register

			char *mem = op1;      // destination memory operand  
			char *reg = op2;      // source register

			// Remove the brackets from memory operand
			char mem_copy[64];
			strcpy(mem_copy, mem);

			if (mem_copy[0] == '[') {
				// Remove opening bracket
				memcpy(mem_copy, mem_copy + 1, strlen(mem_copy));
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
				char base_reg[10] = "";
				char index_reg[10] = "";
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

				unsigned char sib = (scale_enc << 6) | (index_code << 3) | base_code;

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
					machine[len++] = disp & 0xFF;
				} else if (mod == 2 || strlen(base_reg) == 0) {
					// 32-bit displacement for large disp or no-base SIB
					machine[len++] = disp & 0xFF;
					machine[len++] = (disp >> 8) & 0xFF;
					machine[len++] = (disp >> 16) & 0xFF;
					machine[len++] = (disp >> 24) & 0xFF;
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
						machine[len++] = disp & 0xFF;
						machine[len++] = (disp >> 8) & 0xFF;
						machine[len++] = (disp >> 16) & 0xFF;
						machine[len++] = (disp >> 24) & 0xFF;
					}
				} else {
					// [reg+disp]
					char base_reg[10];
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
						machine[len++] = disp & 0xFF;
					} else if (mod == 2) {
						machine[len++] = disp & 0xFF;
						machine[len++] = (disp >> 8) & 0xFF;
						machine[len++] = (disp >> 16) & 0xFF;
						machine[len++] = (disp >> 24) & 0xFF;
					}
				}
			}
		}

		printf("%-20s   ", original);
		for (int i = 0; i < len; i++)
			printf("%02X ", machine[i]);
		printf("\n");

		}	
		else {
			printf("Not found in CSV: %s %s\n", mnemonic, type);
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
		//Skip until word appears once
		if (!skip(fp, "main:")) {
			printf("Word not found\n");
			fclose(fp);
			return;
		}
		fgets(line, sizeof(line), fp);
		while (fgets(line, sizeof(line), fp)) {

			// Trim newline if needed
			line[strcspn(line, "\n")] = 0;
			if (line[0] == '\0') continue; // ignore empty lines
						       //Send each line to Assembly_line()
			Assembly_line(line);
		}

		fclose(fp);
	}

	int main() {
		assembly_file("input.asm");
		return 0;
	}

