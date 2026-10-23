#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdio.h>

#define NP 4
#define INSTRUCTION_MEM_SIZE 256
#define DATA_MEM_SIZE 4096

// physical memory management.
#define MEMSIZE 8192
#define PAGESIZE 512
#define NUM_PHYSICAL_PAGES (MEMSIZE / PAGESIZE)
#define NUM_LOGICAL_PAGES (((INSTRUCTION_MEM_SIZE + PAGESIZE - 1) / PAGESIZE) + ((DATA_MEM_SIZE + PAGESIZE - 1) / PAGESIZE))

// Logical memories are private to each processor/task through page tables.
extern unsigned char Instruction[NP][INSTRUCTION_MEM_SIZE];
extern unsigned char Data[NP][DATA_MEM_SIZE];

// physical memory and page-management state.
extern unsigned char memory[MEMSIZE];
extern unsigned char pageTable[NP][NUM_LOGICAL_PAGES];
extern unsigned char freePages[NUM_PHYSICAL_PAGES];

int initialize_memory(int processor_id, const char *program_file, const char *data_file);
int32_t read32(int processor_id, int address);
void write32(int processor_id, int address, int32_t value);
int finalize_memory(int processor_id, const char *data_file);

// Physical-memory / paging interface used by the OS and processor.
int getPhysicallAddress(int processor_id, int isFetch, int address);
int getFreePage(void);
int allocate_process_pages(int processor_id, int instruction_bytes, int data_bytes);
void free_process_pages(int processor_id);
unsigned char read_instruction_byte(int processor_id, int address);
void memory_system_init(void);
void clear_memory(int processor_id);
void log_page_table(int processor_id, FILE *log);
void log_page_translation(int processor_id, int isFetch, int logical_address, int physical_address, FILE *log);

#endif
