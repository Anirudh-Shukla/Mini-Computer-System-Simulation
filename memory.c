#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "memory.h"

// Lab 5 memory: logical memories are translated through per-task page tables.
unsigned char Instruction[NP][INSTRUCTION_MEM_SIZE];
unsigned char Data[NP][DATA_MEM_SIZE];

// Shared physical memory
unsigned char memory[MEMSIZE];

// Page 0 holds instructions; pages 1..8 hold the logical data image.
unsigned char pageTable[NP][NUM_LOGICAL_PAGES];
unsigned char freePages[NUM_PHYSICAL_PAGES];
static int loaded_data_bytes[NP];
static unsigned char backingData[NP][DATA_MEM_SIZE];

static int valid_processor(int processor_id)
{
    return processor_id >= 0 && processor_id < NP;
}

static int pages_for_bytes(int bytes)
{
    if (bytes <= 0)
        return 1;
    return (bytes + PAGESIZE - 1) / PAGESIZE;
}

void memory_system_init(void)
{
    memset(memory, 0, sizeof(memory));
    memset(freePages, 0, sizeof(freePages));

    for (int p = 0; p < NP; p++)
    {
        for (int i = 0; i < NUM_LOGICAL_PAGES; i++)
            pageTable[p][i] = 0xFF;

        memset(Instruction[p], 0, INSTRUCTION_MEM_SIZE);
        memset(Data[p], 0, DATA_MEM_SIZE);
        memset(backingData[p], 0, DATA_MEM_SIZE);
        loaded_data_bytes[p] = 0;
    }
}

void clear_memory(int processor_id)
{
    if (!valid_processor(processor_id))
    {
        printf("Invalid processor ID: %d\n", processor_id);
        return;
    }

    /* Release any pages previously owned by this processor/task. */
    free_process_pages(processor_id);

    memset(Instruction[processor_id], 0, INSTRUCTION_MEM_SIZE);
    memset(Data[processor_id], 0, DATA_MEM_SIZE);
    memset(backingData[processor_id], 0, DATA_MEM_SIZE);
    loaded_data_bytes[processor_id] = 0;
}

void log_page_table(int processor_id, FILE *log)
{
    if (!valid_processor(processor_id) || log == NULL)
        return;

    fprintf(log, "PAGE TABLE | Processor %d\n", processor_id);
    fprintf(log, "Logical Page | Physical Page | Status\n");
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++)
    {
        unsigned int physical = pageTable[processor_id][i];
        if (physical == 0xFF)
            fprintf(log, "%12d | %13s | NOT MAPPED\n", i, "-");
        else
            fprintf(log, "%12d | %13u | MAPPED\n", i, physical);
    }

    fprintf(log, "Physical Pages Used | ");
    int used = 0;
    for (int i = 0; i < NUM_PHYSICAL_PAGES; i++)
        if (freePages[i])
            used++;
    fprintf(log, "%d / %d\n", used, NUM_PHYSICAL_PAGES);
    fflush(log);
}

void log_page_translation(int processor_id, int isFetch, int logical_address, int physical_address, FILE *log)
{
    if (!valid_processor(processor_id) || log == NULL || physical_address < 0)
        return;

    int instruction_pages = (INSTRUCTION_MEM_SIZE + PAGESIZE - 1) / PAGESIZE;
    int logical_page = isFetch ? logical_address / PAGESIZE
                               : instruction_pages + logical_address / PAGESIZE;
    int offset = logical_address % PAGESIZE;
    int physical_page = physical_address / PAGESIZE;

    fprintf(log, "TRANSLATION | Processor %d | %s | Logical Address: %d | Logical Page: %d | Offset: %d | Physical Page: %d | Physical Address: %d\n",
            processor_id, isFetch ? "INSTRUCTION" : "DATA", logical_address,
            logical_page, offset, physical_page, physical_address);
    fflush(log);
}

int getFreePage(void)
{
    for (int i = 0; i < NUM_PHYSICAL_PAGES; i++)
    {
        if (freePages[i] == 0)
        {
            freePages[i] = 1;
            memset(&memory[i * PAGESIZE], 0, PAGESIZE);
            return i;
        }
    }

    printf("ERROR: No free physical page available.\n");
    return -1;
}

void free_process_pages(int processor_id)
{
    if (!valid_processor(processor_id))
        return;

    for (int i = 0; i < NUM_LOGICAL_PAGES; i++)
    {
        unsigned int physical_page = pageTable[processor_id][i];

        if (physical_page != 0xFF && physical_page < NUM_PHYSICAL_PAGES)
        {
            freePages[physical_page] = 0;
            memset(&memory[physical_page * PAGESIZE], 0, PAGESIZE);
        }

        pageTable[processor_id][i] = 0xFF;
    }
}

int allocate_process_pages(int processor_id, int instruction_bytes, int data_bytes)
{
    if (!valid_processor(processor_id))
        return 0;

    int instruction_pages = pages_for_bytes(instruction_bytes);
    int data_pages = pages_for_bytes(data_bytes);

    if (instruction_pages > (INSTRUCTION_MEM_SIZE + PAGESIZE - 1) / PAGESIZE)
        return 0;

    if (data_pages > (DATA_MEM_SIZE + PAGESIZE - 1) / PAGESIZE)
        return 0;

    /* Start clean. */
    free_process_pages(processor_id);

    /*
     * Lab 5 uses demand paging for data.  Initially map only the pages
     * required to start the process.  Additional data pages are mapped by
     * getPhysicallAddress() when the program actually touches them.
     * This allows several processors to coexist in the 8 KB physical memory.
     */
    for (int i = 0; i < instruction_pages; i++)
    {
        int physical = getFreePage();
        if (physical < 0)
        {
            free_process_pages(processor_id);
            return 0;
        }
        pageTable[processor_id][i] = (unsigned char)physical;
    }

    int data_base = (INSTRUCTION_MEM_SIZE + PAGESIZE - 1) / PAGESIZE;

    /* Map one data page initially; remaining pages are demand mapped. */
    int initial_data_pages = data_pages > 0 ? 1 : 0;
    for (int i = 0; i < initial_data_pages; i++)
    {
        int logical = data_base + i;
        int physical = getFreePage();
        if (physical < 0)
        {
            free_process_pages(processor_id);
            return 0;
        }
        pageTable[processor_id][logical] = (unsigned char)physical;
        memcpy(&memory[physical * PAGESIZE],
               &backingData[processor_id][i * PAGESIZE],
               PAGESIZE);
    }

    return 1;
}

int getPhysicallAddress(int processor_id, int isFetch, int address)
{
    if (!valid_processor(processor_id))
    {
        printf("Invalid processor ID: %d\n", processor_id);
        return -1;
    }

    int logical_limit = isFetch ? INSTRUCTION_MEM_SIZE : DATA_MEM_SIZE;
    if (address < 0 || address >= logical_limit)
    {
        printf("Processor %d: invalid logical %s address %d\n",
               processor_id, isFetch ? "instruction" : "data", address);
        return -1;
    }

    int instruction_pages = (INSTRUCTION_MEM_SIZE + PAGESIZE - 1) / PAGESIZE;
    int logical_page;

    if (isFetch)
        logical_page = address / PAGESIZE;
    else
        logical_page = instruction_pages + address / PAGESIZE;

    if (logical_page < 0 || logical_page >= NUM_LOGICAL_PAGES)
    {
        printf("Processor %d: invalid logical page %d\n", processor_id, logical_page);
        return -1;
    }

    unsigned int physical_page = pageTable[processor_id][logical_page];

    /* Demand-map data pages when first accessed. */
    if (physical_page == 0xFF && !isFetch)
    {
        int physical = getFreePage();
        if (physical < 0)
        {
            printf("Processor %d: page fault - no free physical page.\n",
                   processor_id);
            return -1;
        }

        pageTable[processor_id][logical_page] = (unsigned char)physical;
        printf("Processor %d: PAGE FAULT | Logical Page %d -> Physical Page %d\n",
               processor_id, logical_page, physical);
        memcpy(&memory[physical * PAGESIZE],
               &backingData[processor_id][(logical_page - instruction_pages) * PAGESIZE],
               PAGESIZE);
        physical_page = (unsigned int)physical;
    }

    if (physical_page == 0xFF || physical_page >= NUM_PHYSICAL_PAGES)
    {
        printf("Processor %d: page fault/unmapped logical page %d\n",
               processor_id, logical_page);
        return -1;
    }

    int physical_address = (int)(physical_page * PAGESIZE + (address % PAGESIZE));
    return physical_address;
}

unsigned char read_instruction_byte(int processor_id, int address)
{
    int physical = getPhysicallAddress(processor_id, 1, address);
    if (physical < 0)
        return 0;
    return memory[physical];
}

int initialize_memory(int processor_id, const char *program_file, const char *data_file)
{
    if (!valid_processor(processor_id))
    {
        printf("Invalid processor ID: %d\n", processor_id);
        return 0;
    }

    if (program_file == NULL || data_file == NULL)
    {
        printf("Invalid program or data file.\n");
        return 0;
    }

    FILE *fp = fopen(program_file, "r");
    if (fp == NULL)
    {
        printf("Cannot open program file: %s\n", program_file);
        return 0;
    }

    int value;
    int instruction_bytes = 0;
    while (fscanf(fp, "%x", &value) == 1)
    {
        if (value < 0 || value > 255)
        {
            printf("Invalid instruction byte: %X\n", value);
            fclose(fp);
            return 0;
        }
        instruction_bytes++;
    }
    fclose(fp);

    if (instruction_bytes <= 0)
        instruction_bytes = 4;
    if (instruction_bytes > INSTRUCTION_MEM_SIZE)
    {
        printf("Instruction memory overflow for processor %d.\n", processor_id);
        return 0;
    }

    /* Load the complete logical data image into a host-side backing store. */
    memset(backingData[processor_id], 0, DATA_MEM_SIZE);
    fp = fopen(data_file, "r");
    if (fp == NULL)
    {
        printf("Cannot open data file: %s\n", data_file);
        return 0;
    }

    int data_bytes = 0;
    while (fscanf(fp, "%x", &value) == 1)
    {
        if (value < 0 || value > 255)
        {
            printf("Invalid data byte: %X\n", value);
            fclose(fp);
            return 0;
        }
        if (data_bytes >= DATA_MEM_SIZE)
        {
            printf("Data memory overflow for processor %d.\n", processor_id);
            fclose(fp);
            return 0;
        }
        backingData[processor_id][data_bytes] = (unsigned char)value;
        data_bytes++;
    }
    fclose(fp);

    if (data_bytes <= 0)
        data_bytes = 4;

    /* A previous finalized file may contain the full 4096-byte logical image.
       Only the pages actually needed are resident initially. */
    if (!allocate_process_pages(processor_id, instruction_bytes, data_bytes))
    {
        printf("Processor %d: unable to allocate physical pages.\n", processor_id);
        return 0;
    }

    memset(Instruction[processor_id], 0, INSTRUCTION_MEM_SIZE);
    memset(Data[processor_id], 0, DATA_MEM_SIZE);
    memcpy(Data[processor_id], backingData[processor_id], DATA_MEM_SIZE);

    /* Load program into its mapped physical instruction page(s). */
    fp = fopen(program_file, "r");
    if (fp == NULL)
    {
        free_process_pages(processor_id);
        printf("Cannot open program file: %s\n", program_file);
        return 0;
    }

    int index = 0;
    while (fscanf(fp, "%x", &value) == 1)
    {
        int physical = getPhysicallAddress(processor_id, 1, index);
        if (physical < 0)
        {
            fclose(fp);
            free_process_pages(processor_id);
            return 0;
        }
        memory[physical] = (unsigned char)value;
        Instruction[processor_id][index] = (unsigned char)value;
        index++;
    }
    fclose(fp);
    loaded_data_bytes[processor_id] = data_bytes;

    printf("\nMEMORY INITIALIZED\n");
    printf("Processor : %d\n", processor_id);
    printf("Program : %s\n", program_file);
    printf("Data : %s\n", data_file);
    printf("Instruction pages allocated : %d\n", pages_for_bytes(instruction_bytes));
    printf("Data pages initially mapped : 1\n");

    return 1;
}

int32_t read32(int processor_id, int address)
{
    uint32_t value = 0;

    for (int i = 0; i < 4; i++)
    {
        int physical = getPhysicallAddress(processor_id, 0, address + i);
        if (physical < 0)
            return 0;
        value |= ((uint32_t)memory[physical]) << (8 * i);
    }

    return (int32_t)value;
}

void write32(int processor_id, int address, int32_t value)
{
    uint32_t v = (uint32_t)value;

    for (int i = 0; i < 4; i++)
    {
        int physical = getPhysicallAddress(processor_id, 0, address + i);
        if (physical < 0)
            return;
        memory[physical] = (unsigned char)((v >> (8 * i)) & 0xFFU);
        Data[processor_id][address + i] = memory[physical];
        backingData[processor_id][address + i] = memory[physical];
    }
}

int finalize_memory(int processor_id, const char *data_file)
{
    if (!valid_processor(processor_id))
    {
        printf("Invalid processor ID: %d\n", processor_id);
        return 0;
    }

    if (data_file == NULL)
    {
        printf("Invalid data file.\n");
        return 0;
    }

    FILE *fp = fopen(data_file, "w");
    if (fp == NULL)
    {
        printf("Cannot write data file: %s\n", data_file);
        return 0;
    }

    int output_bytes = loaded_data_bytes[processor_id];
    if (output_bytes <= 0)
        output_bytes = 4;
    if (output_bytes > DATA_MEM_SIZE)
        output_bytes = DATA_MEM_SIZE;
    output_bytes = ((output_bytes + 3) / 4) * 4;

    for (int i = 0; i < output_bytes; i += 4)
    {
        fprintf(fp, "%02X %02X %02X %02X\n",
                Data[processor_id][i],
                Data[processor_id][i + 1],
                Data[processor_id][i + 2],
                Data[processor_id][i + 3]);
    }

    fclose(fp);

    printf("\nMEMORY FINALIZED\n");
    printf("Processor : %d\n", processor_id);
    printf("Data      : %s\n", data_file);

    free_process_pages(processor_id);
    return 1;
}
