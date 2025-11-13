#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <ctype.h>

//helper constants
#define NUMREGS 6 
#define MEM 256

// enum for switch for 8 commands
// typedef to directly refer to instructions
// https://www.geeksforgeeks.org/c/enumeration-enum-c/ 
typedef enum{
	MOV,
	ADD_REG,
	ADD_NUM,
	CMP,
	JE,
	JMP,
	LD,
	ST,
	INVALID
}Opcode;

//struct to hold full instruction including opcode
typedef struct{
	Opcode op;
	int rn, rm, num, addr;

	int line_num;

	char full_line[MEM];
}Instr;

//struct to make up cpu which holds:
//the 6 registers R1, R2, ... R6
//byte-addressable 256-Byte local mem
//total number of executed instructions
//total cycle count
//# hits to local mem
//# executed LD/ST instructions
//flag for whether an instr was cached locally
//flag for JE comparison
//program counter to index into the Instr array when made
typedef struct{
	int R[NUMREGS];
	int mem[MEM];
	int num_instr;
	int num_cycles;
	int local_hits;
	int num_ldst;

	bool cached_local[MEM];
	bool last_je;
	
	int pc;
}CPU;

// headers for the helper functions
static bool parse_line(const char *linebuf, Instr *ins); //function to parse each line of the assembly program
static void print_output(const CPU *cpu); //function to print expected output
static void execute_program(CPU *cpu, const Instr *prog, size_t n); //function to run simulator

int main(int argc, char **argv){
	//check for incorrect usage
	if(argc != 2)
	{
		fprintf(stderr, "Usage: ./myISS <assembly_file>\n");
		return 1;
	}

	FILE *pFile;
	pFile = fopen(argv[1], "r");
	if(pFile == NULL)
	{
		perror("Error opening file");
		return 1;
	}
	
	//dynamic array to hold all instructions
	//reference: https://www.geeksforgeeks.org/c/dynamic-array-in-c/
	size_t size = 128;
	size_t n = 0; //temp variable

	Instr *program = (Instr*)malloc(size * sizeof(*program));

	char linebuf[MEM]; //buffer to hold each raw line in assembly file
	int line_num = 0; //keeps track of line number

	while(fgets(linebuf, sizeof(linebuf), pFile))
	{
		line_num++; //we got a line from file
		
		if(linebuf[0] == '\n')
			continue; //empty lines 

		Instr ins;
		//helper function to parse each line and handle the case switch
		if(!parse_line(linebuf, &ins))
		{
			//print: Unknown instruction: <print the instruction> and exit without crashing
			fprintf(stderr, "Unknown instruction: %s\n", linebuf);
			free(program);
			fclose(pFile);
			return 1;
		}

		//dynamic array size was reached by temp, so realloc more space
		if(n == size)
		{
			size *= 2;

			Instr *tmp = (Instr*)realloc(program, size * sizeof(*program));
			if(!tmp){
				free(program);
				fclose(pFile);
				return 1;
			}
			program = tmp;
		}
		program[n++] = ins;
	}
	fclose(pFile);

	//pass through the program to check addr -> line_num (based on line num in beginning of each line in input file)
	for(size_t i = 0; i < n; i++){
		if(program[i].op == JE || program[i].op == JMP){
			int target_line_num = program[i].addr;

			int found = -1; //temp flag to see if we found the wanted addr (based on line number at beginning of lines)
			
			for(size_t j = 0; j < n; j++){
				if(program[j].line_num == target_line_num){
					found = (int)j;
					break;
				}
			}

			if(found >= 0){
				program[i].addr = found;
			}else{
				program[i].addr = (int)n; //if not found exit cleanly
			}
		}
	}

	//run the actual simulator
	CPU cpu;
	memset(&cpu, 0, sizeof(cpu));
	cpu.pc = 0;
	cpu.last_je = false;

	execute_program(&cpu, program, n);

	//print expected output
	print_output(&cpu);

	free(program);
	return 0;
}

//function to parse each line and fill Instr struct
//8 possibilities:
// 	MOV rn, num	puts an 8-bit (positive/negative) integer <num> into Rn (range of num: [-128,127])
// 	ADD rn, rm	performs Rn+Rm, and writes the result in Rn
// 	ADD rn, num	performs Rn+num and writes the result in Rn (range of num: [-128,127])
// 	CMP rn, rm	compares Rn and Rm (typically used together with the JE instruction)
// 	JE address	jumps to instruction at <Address> if the last comparison resulted in equality
// 	JMP address	unconditionally jumps to the instruction at <Address>
// 	LD rn, [rm]	loads from the address stored in Rm into Rn
// 	ST [rm], rn	stores the contents of Rn into the memory address that is in Rm
static bool parse_line(const char *linebuf, Instr *ins)
{
	//copy linebuf into a buffer we can deal with
	char buf[MEM];
	strncpy(buf, linebuf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	
	//keep a copy of the full line in Instr struct
	strncpy(ins->full_line, linebuf, sizeof(ins->full_line) - 1);
	ins->full_line[sizeof(ins->full_line) - 1] = '\0';

	//initialize Instr struct as invalid for now
	ins->op = INVALID;
	ins->rn = -1;
	ins->rm = -1;
	ins->num = 0;
	ins->addr = 0;

	//chop up buf into needed components: first token is the opcode
	//https://www.geeksforgeeks.org/cpp/strtok-strtok_r-functions-c-examples/
	//for some reason, since I am working on a Windows laptop, when i created sample.assembly
	//windows adds \r instead of \n so I was getting unknown lines even though they were valid instructions
	const char *delimiters = " ,[]\n\r\t"; //tokenizes at any of the characters in the delimiters string
	char *token = strtok(buf, delimiters);
	if(!token) //nothing there
		return false;

	ins->line_num = (int)strtol(token, NULL, 10);

	token = strtok(NULL, delimiters);

	// token now includes ONLY the opcode, meaning we can set ins->op
	if(strcmp(token, "MOV") == 0){
		ins->op = MOV;
	}else if(strcmp(token, "ADD") == 0){
		ins->op = ADD_NUM; //for now and later we can check if rm is being used instead of num
	}else if(strcmp(token, "CMP") == 0){
		ins->op = CMP; 
	}else if(strcmp(token, "JE") == 0){
		ins->op = JE;
	}else if(strcmp(token, "JMP") == 0){
		ins->op = JMP;
	}else if(strcmp(token, "LD") == 0){
		ins->op = LD;
	}else if(strcmp(token, "ST") == 0){
		ins->op = ST;
	}else{
		return false; // ins->op already set to INVALID
	}

	//after the opcode, there only up to 2 extra fields for Rn, Rm, num or addr
	//so here i'll just get the 2 (if applicable) fields from buf
	char *field1 = strtok(NULL, delimiters);
	char *field2 = strtok(NULL, delimiters);

	char *toomanyfields = strtok(NULL, delimiters);
	if(toomanyfields)
		return false; // too many (shouldn't happen)

	//fill rest of the fields of Instr struct
	bool ret = false;
	switch(ins->op){
		case MOV: // MOV Rn, <num>
			if(!field1 || !field2)
				break;
			ins->rn = (int)(field1[1] - '1');
			if(ins->rn < 0)
				break;

			char *end = NULL;
			long n = strtol(field2, &end, 10);
			
			ins->num = (int)n;
			ret = true;

			break;

		case ADD_NUM: // ADD Rn, Rm || ADD Rn, <num>
			if(!field1 || !field2)
				break;
			ins->rn = (int)(field1[1] - '1');
			if(ins->rn < 0)
				break;

			//check if the second field's first character is 'R', if not its num
			if(field2[0] == 'R'){
				ins->op = ADD_REG;
				ins->rm = (int)(field2[1] - '1');
				ret = true;
			}else{
				ins->op = ADD_NUM;
				
				char *end = NULL;
				long n4 = strtol(field2, &end, 10);

				ins->num = (int)n4;
				ret = true;
			}

			break;

		case CMP: // CMP Rn, Rm
			if(!field1 || !field2)
				break;
			ins->rn = (int)(field1[1] - '1');
			if(ins->rn < 0)
				break;

			ins->rm = (int)(field2[1] - '1');
			if(ins->rm < 0)
				break;
			
			ret = true;
			break;

		case JE: // JE <Address>
			if(!field1 || field2)
				break;

			char *end2 = NULL;
			long n2 = strtol(field1, &end2, 10);
			if(n2 < 0)
				break;

			ins->addr = (int)n2;
			
			ret = true;
			break;

		case JMP: // JMP <Address>
			if(!field1 || field2)
				break;

			char *end3 = NULL;
			long n3 = strtol(field1, &end3, 10);
			if(n3 < 0)
				break;

			ins->addr = (int)n3;
			
			ret = true;
			break;

		case LD: // LD Rn, [Rm]
			if(!field1 || !field2)
				break;

			ins->rn = (int)(field1[1] - '1');
			if(ins->rn < 0)
				break;

			ins->rm = (int)(field2[1] - '1');
			if(ins->rm < 0)
				break;

			ret = true;			
			break;

		case ST: // ST [Rm], Rn
			if(!field1 || !field2)
				break;

			ins->rm = (int)(field1[1] - '1');
			if(ins->rm < 0)
				break;

			ins->rn = (int)(field2[1] - '1');
			if(ins->rn < 0)
				break;
			
			ret = true;
			break;

	}

	return ret;
}

//function to use struct Instr (now filled by parse_line) &
//initialized "CPU"  to go through and fill CPU struct
static void execute_program(CPU *cpu, const Instr *prog, size_t n)
{
	// keep executing while program counter (pc) is within 0 & n
	while(cpu->pc >= 0 && (size_t)cpu->pc < n){
		// get the wanted instruction from the program
		const Instr *ins = &prog[cpu->pc];
		// increment num_instr since we executed an instruction
		cpu->num_instr++;

		//based on the opcode, simulate instruction
		// keep in mind each register has 8 bits (signed)
		switch(ins->op){
			case MOV:{
				int8_t num8bit = (int8_t)ins->num;
				cpu->R[ins->rn] = num8bit;
				// MOV = 1 clock cycle
				cpu->num_cycles += 1;
				cpu->pc += 1;
				 }break;

			case ADD_REG:{
				//Rn = Rn + Rm
				int sum = (cpu->R[ins->rn] & 0xFF) + (cpu->R[ins->rm] & 0xFF);
				cpu->R[ins->rn] = (int8_t)(sum & 0xFF);

				// ADD = 1 clock cycle
				cpu->num_cycles += 1;
				cpu->pc += 1;
				     }break;

			case ADD_NUM:{
				// Rn = Rn + num
				int sum = (cpu->R[ins->rn] & 0xFF) + (int8_t)ins->num;
				cpu->R[ins->rn] = (int8_t)(sum & 0xFF);

				cpu->num_cycles += 1;
				cpu->pc += 1;
				     }break;

			case CMP:{
				bool temp = ((cpu->R[ins->rn] & 0xFF) == (cpu->R[ins->rm] & 0xFF));
				cpu->last_je = temp;

				// CMP = 1 clock cycle
				cpu->num_cycles += 1;
				cpu->pc += 1;
				 }break;

			case JE:{
				cpu->num_cycles += 1;
				// if last_je == true, jump to instruction addr
				if(cpu->last_je){
					if(ins->addr < 0 || (size_t)ins->addr >= n){
						cpu->pc = (int)n; // exit cleanly
					}else{
						cpu->pc = ins->addr;
					}
				}else{
					cpu->pc += 1;
				}
				}break;

			case JMP:{
				cpu->num_cycles += 1;
				if(ins->addr < 0 || (size_t)ins->addr >= n){
					cpu->pc = (int)n;
				}else{
					cpu->pc = ins->addr;
				}
				 }break;

			case LD:{
				// LD = 50 if external mem (first time touched), 2 cycles if in local mem
				cpu->num_ldst += 1;
				
				int addr = (cpu->R[ins->rm] & 0xFF);

				if(cpu->cached_local[addr]){
					cpu->num_cycles += 2;
					cpu->local_hits += 1;
				}else{
					cpu->num_cycles += 50;
					cpu->cached_local[addr] = true;
				}

				cpu->R[ins->rn] = (cpu->mem[addr] & 0xFF);
				cpu->pc += 1;
				}break;

			case ST:{
				// ST is the same as LD
				cpu->num_ldst += 1;

				int addr2 = (cpu->R[ins->rm] & 0xFF);
				if(cpu->cached_local[addr2]){
					cpu->num_cycles += 2;
					cpu->local_hits += 1;
				}else{
					cpu->num_cycles += 50;
					cpu->cached_local[addr2] = true;
				}

				cpu->mem[addr2] = (cpu->R[ins->rn] & 0xFF);
				cpu->pc += 1;
				}break;

			default:
				return;
		}
	}
}

//function to print expected output
static void print_output(const CPU *cpu)
{
	printf("Total number of executed instructions: %d\n", cpu->num_instr);
	printf("Total number of clock cycles: %d\n", cpu->num_cycles);
	printf("Number of hits to local memory: %d\n", cpu->local_hits);
	printf("Total number of executed LD/ST instructions: %d\n", cpu->num_ldst);
}
