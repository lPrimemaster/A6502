#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

// Cycles counting is not accurate

#define MAX_MEM 1024 * 64

typedef unsigned char  BYTE; //  8-bit
typedef unsigned short WORD; // 16-bit

typedef unsigned int u32;

#define LA 0
#define LX 1
#define LY 2

int PLAT_LE = 1;
int is_big_endian()
{
	union {
		u32 i;
		char c[4];
	} e = { 0x01000000 };

	return e.c[0];
}

void SwapWORDBytes(WORD* w)
{
	*w = ((*w << 8) & 0xff00) | ((*w >> 8) & 0x00ff);
}

typedef struct _cpu
{
	WORD PC; // The program counter
	BYTE SP; // The stack pointer

	BYTE A, X, Y; // Accumulator and X, Y registers

	BYTE C : 1; //     Carry flag
	BYTE Z : 1; //      Zero flag
	BYTE I : 1; // Interrupt flag
	BYTE D : 1; //   Decimal flag
	BYTE B : 1; //     Break flag
	BYTE V : 1; //  Overflow flag
	BYTE N : 1; //  Negative flag

}CPU;

typedef struct _mem
{
	BYTE* Data;
}MEM;

// Initializes memory
void initializeMemory(MEM* mem)
{
	mem->Data = calloc(MAX_MEM, 1);

	if (mem->Data == NULL)
	{
		printf("Failed mem initialize.\n");
	}
}

// Deletes memory
void deleteMemory(MEM* mem)
{
	if(mem->Data != NULL)
		free(mem->Data);
}

// Resets the cpu
void resetAll(CPU* cpu, MEM* mem)
{
	cpu->PC = 0xFFFC; // 6502 Default reset vector

	cpu->SP = 0xFF; // 6502 Default start of SP

	cpu->A = cpu->Y = cpu->X = 0;
	cpu->C = 0;
	cpu->Z = 0;
	cpu->I = 0;
	cpu->D = 0;
	cpu->B = 0;
	cpu->V = 0;
	cpu->N = 0;

	initializeMemory(mem);

	// Hack reset vector
	mem->Data[0xFFFC] = 0x4C; // Jump absolute
}

BYTE fetch8(u32* cycles, CPU* cpu, MEM* mem)
{
	(*cycles)++;
	return *(mem->Data + cpu->PC++); 
}

WORD fetch16(u32* cycles, CPU* cpu, MEM* mem)
{
	(*cycles) += 2;
	WORD RET = *(mem->Data + cpu->PC++);
	RET |= ((*(mem->Data + cpu->PC++)) << 8);

	if (!PLAT_LE) SwapWORDBytes(&RET);

	return RET;
}

void write8(u32* cycles, BYTE value, u32 address, MEM* mem)
{
	(*cycles)++;
	*(mem->Data + address) = value;
}

void write16(u32* cycles, WORD value, u32 address, MEM* mem)
{
	(*cycles) += 2;
	*(mem->Data + address)     = value & 0xFF; 
	*(mem->Data + address + 1) =   value >> 8;
}

BYTE read8(u32* cycles, u32 address, MEM* mem)
{
	(*cycles)++;
	return *(mem->Data + address);
}

BYTE add8(u32* cycles, BYTE a, BYTE b)
{
	(*cycles)++;
	return a + b; // Sum wraps arround by default
}

WORD add16(u32* cycles, WORD a, WORD b)
{
	(*cycles) += 2;
	return a + b; // Sum wraps arround by default
}

BYTE IMM(u32* cycles, CPU* cpu, MEM* mem)
{
	return fetch8(cycles, cpu, mem);
}

BYTE ZP0(u32* cycles, CPU* cpu, MEM* mem)
{
	BYTE ZP_ADDRESS = fetch8(cycles, cpu, mem);
	return read8(cycles, ZP_ADDRESS, mem);
}

BYTE ZPX(u32* cycles, CPU* cpu, MEM* mem)
{
	BYTE ZP_ADDRESS = fetch8(cycles, cpu, mem);
	BYTE ZPX_ADDRESS = add8(cycles, ZP_ADDRESS, cpu->X);
	return read8(cycles, ZPX_ADDRESS, mem);
}

BYTE ZPY(u32* cycles, CPU* cpu, MEM* mem)
{
	BYTE ZP_ADDRESS = fetch8(cycles, cpu, mem);
	BYTE ZPX_ADDRESS = add8(cycles, ZP_ADDRESS, cpu->Y);
	return read8(cycles, ZPX_ADDRESS, mem);
}

WORD AB0(u32* cycles, CPU* cpu, MEM* mem)
{
	return fetch16(cycles, cpu, mem);
}

WORD ABX(u32* cycles, CPU* cpu, MEM* mem)
{
	WORD AB_ADDRESS = fetch16(cycles, cpu, mem);
	WORD ABX_ADDRESS = add16(cycles, AB_ADDRESS, cpu->X);
	return read8(cycles, ABX_ADDRESS, mem);
}

WORD ABY(u32* cycles, CPU* cpu, MEM* mem)
{
	WORD AB_ADDRESS = fetch16(cycles, cpu, mem);
	WORD ABY_ADDRESS = add16(cycles, AB_ADDRESS, cpu->Y);
	return read8(cycles, ABY_ADDRESS, mem);
}

void LoadRegister(u32 reg, CPU* cpu, BYTE value)
{
	switch (reg)
	{
	case LA: cpu->A = value; break;
	case LX: cpu->X = value; break;
	case LY: cpu->Y = value; break;
	}
}

void WritePC(u32* cycles, CPU* cpu, WORD value)
{
	cpu->PC = value;
	(*cycles)++;
}

void SetFlags_ZN(CPU* cpu)
{
	cpu->Z = (cpu->A == 0);
	cpu->N = (cpu->A & 0b10000000) > 0;
}

void PushWordToStack(u32* cycles, CPU* cpu, MEM* mem, WORD value)
{
	write8(cycles, value >> 8, cpu->SP, mem);
	cpu->SP--;

	write8(cycles, value & 0x00FF, cpu->SP, mem);
	cpu->SP--;
}

WORD PullWordFromStack(u32* cycles, CPU* cpu, MEM* mem)
{
	WORD lo = read8(cycles, ++(cpu->SP), mem);
	WORD hi = read8(cycles, ++(cpu->SP), mem) << 8;
	return lo | hi;
}

// Instructions
#define INS_LDA_IMM  0xA9
#define INS_LDA_ZP0  0xA5
#define INS_LDA_ZPX  0xB5
#define INS_LDA_AB0  0xAD
#define INS_LDA_ABX  0xBD
#define INS_LDA_ABY  0xB9

#define INS_LDX_IMM  0xA2
#define INS_LDX_ZP0  0xA6
#define INS_LDX_ZPY  0xB6
#define INS_LDX_AB0  0xAE
#define INS_LDX_ABY  0xBE

#define INS_LDY_IMM  0xA0
#define INS_LDY_ZP0  0xA4
#define INS_LDY_ZPX  0xB4
#define INS_LDY_AB0  0xAC
#define INS_LDY_ABX  0xBC

#define INS_JMP_ABS  0x4C
#define INS_JMP_IND  0x6C
#define INS_JSR      0x20
#define INS_RTS      0x60

u32 exec(CPU* cpu, MEM* mem)
{
	u32 cycles = 0;
	while (1)
	{
		BYTE I = IMM(&cycles, cpu, mem);

		switch (I)
		{

		// LDA
		case INS_LDA_IMM:
		{
			BYTE V = IMM(&cycles, cpu, mem);
			LoadRegister(LA, cpu, V);
		} break;
		case INS_LDA_ZP0:
		{
			BYTE V = ZP0(&cycles, cpu, mem);
			LoadRegister(LA, cpu, V);
		} break;
		case INS_LDA_ZPX:
		{
			BYTE V = ZPX(&cycles, cpu, mem);
			LoadRegister(LA, cpu, V);
		} break;
		case INS_LDA_AB0:
		{
			WORD ADDRESS = AB0(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LA, cpu, V);
		} break;
		case INS_LDA_ABX:
		{
			WORD ADDRESS = ABX(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LA, cpu, V);
		} break;
		case INS_LDA_ABY:
		{
			WORD ADDRESS = ABY(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LA, cpu, V);
		} break;

		// LDX
		case INS_LDX_IMM:
		{
			BYTE V = IMM(&cycles, cpu, mem);
			LoadRegister(LX, cpu, V);
		} break;
		case INS_LDX_ZP0:
		{
			BYTE V = ZP0(&cycles, cpu, mem);
			LoadRegister(LX, cpu, V);
		} break;
		case INS_LDX_ZPY:
		{
			BYTE V = ZPY(&cycles, cpu, mem);
			LoadRegister(LX, cpu, V);
		} break;
		case INS_LDX_AB0:
		{
			WORD ADDRESS = AB0(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LX, cpu, V);
		} break;
		case INS_LDX_ABY:
		{
			WORD ADDRESS = ABY(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LX, cpu, V);
		} break;

		// LDY
		case INS_LDY_IMM:
		{
			BYTE V = IMM(&cycles, cpu, mem);
			LoadRegister(LY, cpu, V);
		} break;
		case INS_LDY_ZP0:
		{
			BYTE V = ZP0(&cycles, cpu, mem);
			LoadRegister(LY, cpu, V);
		} break;
		case INS_LDY_ZPX:
		{
			BYTE V = ZPX(&cycles, cpu, mem);
			LoadRegister(LY, cpu, V);
		} break;
		case INS_LDY_AB0:
		{
			WORD ADDRESS = AB0(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LY, cpu, V);
		} break;
		case INS_LDY_ABX:
		{
			WORD ADDRESS = ABX(&cycles, cpu, mem);
			BYTE V = read8(&cycles, ADDRESS, mem);
			LoadRegister(LY, cpu, V);
		} break;

		case INS_JSR:
		{
			WORD ADDRESS = AB0(&cycles, cpu, mem);
			PushWordToStack(&cycles, cpu, mem, (cpu->PC - 1));
			WritePC(&cycles, cpu, ADDRESS);
		} break;
		case INS_RTS:
		{
			WORD npc = PullWordFromStack(&cycles, cpu, mem);
			WritePC(&cycles, cpu, ++npc);
		} break;
		case INS_JMP_ABS:
		{
			WORD ADDRESS = AB0(&cycles, cpu, mem);
			WritePC(&cycles, cpu, ADDRESS);
		} break;
		default:
			printf("Instruction not recognized = 0x%02x [Maybe program execution ended?]\n", I);
			return cycles;
		}
	}
}

static void load_bytecode(const BYTE* code, size_t nbytes, MEM* mem, CPU* cpu)
{
	WORD bcld_adress_lo = code[0];
	WORD bcld_adress_hi = code[1];

	WORD bcld_adress = bcld_adress_lo | (bcld_adress_hi << 8);

	u32 CP = 2;

	// TODO : Change this to a proper reset vector behaviour
	mem->Data[0xFFFD] = (BYTE)bcld_adress_lo;
	mem->Data[0xFFFE] = (BYTE)bcld_adress_hi;

	for (WORD a = bcld_adress; a < bcld_adress + nbytes - 2; a++)
	{
		mem->Data[a] = code[CP++];
	}
}

static BYTE* load_file(const char* file, MEM* mem, size_t* size)
{
	FILE* f = NULL;
	BYTE* b = NULL;
	f = fopen(file, "rb");
	if (f != NULL)
	{
		fseek(f, 0L, SEEK_END);
		size_t sz = ftell(f);
		*size = sz;
		rewind(f);

		b = malloc(sizeof(BYTE) * sz);

		if (b != NULL)
		{
			fread(b, sizeof(BYTE), sz, f);

			printf("Loading memory bytes:\n");

			for (int i = 0; i < sz; i++)
			{
				printf("0x%04x ", b[i]);

				if (i % 10 == 0 && i > 0)
					putchar('\n');
			}
			printf("\n\n\n");
		}
		fclose(f);
	}
	return b;
}

static void load_program(const char* file, MEM* mem, CPU* cpu)
{
	size_t fsz;
	BYTE* bc = load_file(file, mem, &fsz);

	if (bc != NULL)
	{
		load_bytecode(bc, fsz, mem, cpu);
		free(bc);
	}
}

int main(int argc, char* argv[])
{
	PLAT_LE = !is_big_endian();
	CPU cpu;
	MEM mem;
	resetAll(&cpu, &mem);

	if (argc > 1)
	{
		load_program(argv[1], &mem, &cpu);

		exec(&cpu, &mem);

		printf("Acc val: 0x%08x\n", cpu.A);
		printf("X val: 0x%08x\n", cpu.X);
	}

	deleteMemory(&mem);
	return 0;
}
