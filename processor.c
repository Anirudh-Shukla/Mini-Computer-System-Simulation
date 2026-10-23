#include <stdio.h>
#include <stdint.h>
#include <string.h>


#include "processor.h"
#include "memory.h"

// WINDOWS VERSION

 #ifdef _WIN32
 #include <windows.h>
 #else
 #include <sys/select.h>
 #endif

// Linux / POSIX version
// #include <unistd.h>
// #include <time.h>

int32_t Register[NP][NUM_REGISTERS];                  // Scalar registers: Register[processor][register]
int32_t VRegister[NP][NUM_VREGISTERS][VREG_ELEMENTS]; // Vector registers: VRegister[processor][vector_register][lane]

int PC[NP];

int Z[NP];
int N[NP];
int C[NP];
int V[NP];

int end_of_simulation[NP];

int proc_id = 0;

static unsigned char opcode;
static unsigned char dest;
static unsigned char src1;
static unsigned char src2;

// All processors/tasks share one log file.
static FILE *fd_log = NULL;

static int valid_processor(int processor_id)
{
    if (processor_id < 0 || processor_id >= NP)
    {
        printf("Invalid processor ID: %d\n", processor_id);
        return 0;
    }

    return 1;
}

// SLEEP
static void processor_sleep(void)
{
    // WINDOWS VERSION
    Sleep(1);

    // Linux replacement:
    // nanosleep() is POSIX and works with GCC on Linux.
    // struct timespec ts;
    // ts.tv_sec = 0;
    // ts.tv_nsec = 1000000L; // 1 millisecond
    // nanosleep(&ts, NULL);
}

// RESET
void reset(void)
{
    int i;
    int j;
    int k;

    if (!valid_processor(proc_id))
        return;

    for (i = 0; i < NUM_REGISTERS; i++)
    {
        Register[proc_id][i] = 0;
    }

    for (i = 0; i < NUM_VREGISTERS; i++)
    {
        for (j = 0; j < VREG_ELEMENTS; j++)
        {
            VRegister[proc_id][i][j] = 0;
        }
    }

    PC[proc_id] = 0;

    Z[proc_id] = 0;
    N[proc_id] = 0;
    C[proc_id] = 0;
    V[proc_id] = 0;

    end_of_simulation[proc_id] = 0;

    opcode = 0;
    dest = 0;
    src1 = 0;
    src2 = 0;

    (void)k;

    printf("Processor %d reset successfully.\n", proc_id);
}

// FETCH
void fetch(void)
{
    int pc;

    if (!valid_processor(proc_id))
        return;

    pc = PC[proc_id];

    /*
     * Every instruction contains four bytes:
     *
     * byte 0 = opcode
     * byte 1 = destination
     * byte 2 = source 1
     * byte 3 = source 2
     */
    if (pc < 0 || pc + 3 >= INSTRUCTION_MEM_SIZE)
    {
        printf("Processor %d: instruction memory overflow at PC=%d\n", proc_id, pc);

        end_of_simulation[proc_id] = 1;
        return;
    }

    opcode = read_instruction_byte(proc_id, pc);
    dest = read_instruction_byte(proc_id, pc + 1);
    src1 = read_instruction_byte(proc_id, pc + 2);
    src2 = read_instruction_byte(proc_id, pc + 3);

    // Record the four byte translations for this instruction fetch.
    if (fd_log != NULL)
    {
        for (int offset = 0; offset < 4; offset++)
        {
            int logical = pc + offset;
            int physical = getPhysicallAddress(proc_id, 1, logical);
            log_page_translation(proc_id, 1, logical, physical, fd_log);
        }
    }

    // Move to next instruction.
    PC[proc_id] += 4;
}

//   DECODE
void decode(void)
{
    // Decode is intentionally empty.
    //  Decode can be extended later if required.
}

//   UPDATE ADD FLAGS
static void updateAddFlags(int32_t a, int32_t b, int32_t result)
{
    uint32_t ua;
    uint32_t ub;
    uint32_t ur;

    ua = (uint32_t)a;
    ub = (uint32_t)b;
    ur = (uint32_t)result;

    // Zero flag.
    Z[proc_id] = (result == 0);

    // Negative flag.
    N[proc_id] = ((uint32_t)result & 0x80000000U) != 0;

    // Carry flag.
    C[proc_id] = (ur < ua) || (ur < ub);

    // Signed overflow.
    V[proc_id] = ((a >= 0 && b >= 0 && result < 0) || (a < 0 && b < 0 && result >= 0));
}

//   UPDATE SUB FLAGS
static void updateSubFlags(int32_t a, int32_t b, int32_t result)
{

    Z[proc_id] = (result == 0);
    N[proc_id] = ((uint32_t)result & 0x80000000U) != 0;
    C[proc_id] = ((uint32_t)a > (uint32_t)b);
    V[proc_id] = ((a >= 0 && b < 0 && result < 0) || (a < 0 && b >= 0 && result >= 0));
}

//   UPDATE VECTOR FLAGS
static void updateVectorFlags(int vector_register)
{
    int i;

    Z[proc_id] = 1;
    N[proc_id] = 0;

    for (i = 0; i < VREG_ELEMENTS; i++)
    {
        if (VRegister[proc_id][vector_register][i] != 0)
            Z[proc_id] = 0;

        if (VRegister[proc_id][vector_register][i] < 0)
            N[proc_id] = 1;
    }
}

static void print_register_value(int register_number)
{
    if (fd_log == NULL)
        return;

    if (register_number < 0 || register_number >= NUM_REGISTERS)
    {
        return;
    }

    fprintf(fd_log, "Processor %d: x%d = %d (0x%08X)\n", proc_id, register_number,
            Register[proc_id][register_number], (uint32_t)Register[proc_id][register_number]);

    fflush(fd_log);
}

//   EXECUTE
void execute(void)
{
    if (!valid_processor(proc_id))
        return;

    switch (opcode)
    {
    // 0x00 : END
    case 0x00:
    {
        end_of_simulation[proc_id] = 1;
        break;
    }

    //  0x01 : ADD
    case 0x01:
    {
        int32_t a;
        int32_t b;
        int32_t result;

        a = Register[proc_id][src1];
        b = Register[proc_id][src2];

        result = a + b;
        Register[proc_id][dest] = result;
        updateAddFlags(a, b, result);

        break;
    }

    // 0x02 : SUB
    case 0x02:
    {
        int32_t a;
        int32_t b;
        int32_t result;

        a = Register[proc_id][src1];
        b = Register[proc_id][src2];

        result = a - b;
        Register[proc_id][dest] = result;
        updateSubFlags(a, b, result);

        break;
    }
    //  0x03 : MUL
    case 0x03:
    {
        int32_t a;
        int32_t b;
        int32_t result;

        a = Register[proc_id][src1];
        b = Register[proc_id][src2];

        result = a * b;
        Register[proc_id][dest] = result;
        Z[proc_id] = (result == 0);
        N[proc_id] = ((uint32_t)result & 0x80000000U) != 0;

        break;
    }

    //   0x04 : DIV
    case 0x04:
    {
        int32_t a;
        int32_t b;
        int32_t result;

        a = Register[proc_id][src1];
        b = Register[proc_id][src2];

        if (b == 0)
        {
            printf("Processor %d: Division by zero\n", proc_id);

            end_of_simulation[proc_id] = 1;
            break;
        }

        result = a / b;
        Register[proc_id][dest] = result;

        Z[proc_id] = (result == 0);
        N[proc_id] = ((uint32_t)result & 0x80000000U) != 0;

        break;
    }

    //  0x05 : INTEGER MEMORY READ
    case 0x05:
    {
        int address;

        address = Register[proc_id][src2];
        Register[proc_id][dest] = read32(proc_id, address);
        Z[proc_id] = (Register[proc_id][dest] == 0);
        N[proc_id] = ((uint32_t)Register[proc_id][dest] & 0x80000000U) != 0;

        break;
    }

        //   0x06 : INTEGER MEMORY WRITE
    case 0x06:
    {
        int address;

        address = Register[proc_id][dest];
        write32(proc_id, address, Register[proc_id][src2]);

        break;
    }

    // 0x07 : DATA MOVEMENT
    case 0x07:
    {
        Register[proc_id][dest] = src1;
        Z[proc_id] = (Register[proc_id][dest] == 0);
        N[proc_id] = ((uint32_t)Register[proc_id][dest] & 0x80000000U) != 0;

        break;
    }

    //  0x08 : PRINT
    case 0x08:
    {
        print_register_value(src2);
        break;
    }

    //   0x09 : ADD CONSTANT
    case 0x09:
    {
        int32_t a;
        int32_t b;
        int32_t result;

        a = Register[proc_id][src1];
        b = (int8_t)src2;

        result = a + b;
        Register[proc_id][dest] = result;
        updateAddFlags(a, b, result);

        break;
    }

    //   0x0A : SUB CONSTANT
    case 0x0A:
    {
        int32_t a;
        int32_t b;
        int32_t result;

        a = Register[proc_id][src1];
        b = (int8_t)src2;

        result = a - b;
        Register[proc_id][dest] = result;
        updateSubFlags(a, b, result);

        break;
    }

    //  0x0B : MUL CONSTANT
    case 0x0B:
    {

        int32_t constant;
        constant = (int8_t)src2;
        Register[proc_id][dest] = Register[proc_id][src1] * constant;
        Z[proc_id] = (Register[proc_id][dest] == 0);
        N[proc_id] = ((uint32_t)Register[proc_id][dest] & 0x80000000U) != 0;

        break;
    }

    //  0x0C : DIV CONSTANT
    case 0x0C:
    {
        int32_t constant;
        constant = (int8_t)src2;
        if (constant == 0)
        {
            printf("Processor %d: Division by zero\n", proc_id);

            end_of_simulation[proc_id] = 1;
            break;
        }

        Register[proc_id][dest] = Register[proc_id][src1] / constant;
        Z[proc_id] = (Register[proc_id][dest] == 0);
        N[proc_id] = ((uint32_t)Register[proc_id][dest] & 0x80000000U) != 0;

        break;
    }

    //  0x0D : READ CONSTANT ADDRESS
    case 0x0D:
    {

        int address;
        address = src2;
        Register[proc_id][dest] = read32(proc_id, address);
        Z[proc_id] = (Register[proc_id][dest] == 0);
        N[proc_id] = ((uint32_t)Register[proc_id][dest] & 0x80000000U) != 0;

        break;
    }

    //  0x0E : WRITE CONSTANT ADDRESS
    case 0x0E:
    {

        int address;
        address = src2;
        write32(proc_id, address, Register[proc_id][src1]);

        break;
    }

    //  0x0F : MOVE SIGNED CONSTANT
    case 0x0F:
    {

        Register[proc_id][dest] = (int8_t)src1;
        Z[proc_id] = (Register[proc_id][dest] == 0);
        N[proc_id] = ((uint32_t)Register[proc_id][dest] & 0x80000000U) != 0;

        break;
    }

    //  0x10 : BEQ
    case 0x10:
    {

        int offset;
        offset = (int8_t)src2;
        if (Z[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x11 : BNE
    case 0x11:
    {

        int offset;
        offset = (int8_t)src2;
        if (!Z[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x12 : BCS
    case 0x12:
    {

        int offset;
        offset = (int8_t)src2;
        if (C[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x13 : BCC
    case 0x13:
    {

        int offset;
        offset = (int8_t)src2;
        if (!C[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x14 : BMI
    case 0x14:
    {

        int offset;
        offset = (int8_t)src2;
        if (N[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x15 : BPL
    case 0x15:
    {

        int offset;
        offset = (int8_t)src2;
        if (!N[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x16 : BVS
    case 0x16:
    {

        int offset;
        offset = (int8_t)src2;
        if (V[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x17 : BVC
    case 0x17:
    {

        int offset;
        offset = (int8_t)src2;
        if (!V[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x18 : BHI
    case 0x18:
    {

        int offset;
        offset = (int8_t)src2;
        if (C[proc_id] && !Z[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x19 : BLS
    case 0x19:
    {

        int offset;
        offset = (int8_t)src2;
        if (!C[proc_id] || Z[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x1A : BGE
    case 0x1A:
    {

        int offset;
        offset = (int8_t)src2;
        if (N[proc_id] == V[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x1B : BLT
    case 0x1B:
    {

        int offset;
        offset = (int8_t)src2;
        if (N[proc_id] != V[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x1C : BGT
    case 0x1C:
    {

        int offset;
        offset = (int8_t)src2;
        if (!Z[proc_id] && N[proc_id] == V[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //  0x1D : BLE
    case 0x1D:
    {
        int offset;
        offset = (int8_t)src2;
        if (Z[proc_id] || N[proc_id] != V[proc_id])
        {
            PC[proc_id] = PC[proc_id] - 4 + offset * 4;
        }

        break;
    }

    //   0x1E : BAL
    case 0x1E:
    {

        int offset;
        offset = (int8_t)src2;
        PC[proc_id] = PC[proc_id] - 4 + offset * 4;

        break;
    }

    //   0x21 : VADD
    case 0x21:
    {
        int i;

        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] + VRegister[proc_id][src2][i];
        }

        updateVectorFlags(dest);

        break;
    }

    //  0x22 : VSUB
    case 0x22:
    {

        int i;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] - VRegister[proc_id][src2][i];
        }

        updateVectorFlags(dest);

        break;
    }

    //   0x23 : VMUL
    case 0x23:
    {

        int i;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] * VRegister[proc_id][src2][i];
        }

        updateVectorFlags(dest);

        break;
    }

    //  0x25 : VECTOR LOAD
    case 0x25:
    {
        int address;
        int i;

        address = Register[proc_id][src2];

        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = read32(proc_id, address);
            address += 4;
        }

        Register[proc_id][src2] = address;
        updateVectorFlags(dest);

        break;
    }

    //   0x26 : VECTOR STORE
    case 0x26:
    {
        int address;
        int i;

        address = Register[proc_id][src2];

        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            write32(proc_id, address, VRegister[proc_id][src1][i]);
            address += 4;
        }

        Register[proc_id][src2] = address;
        break;
    }

    // 0x29 : VECTOR ADD CONSTANT
    case 0x29:
    {
        int32_t constant;
        int i;

        constant = (int8_t)src2;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] + constant;
        }

        updateVectorFlags(dest);

        break;
    }

    // 0x2A : VECTOR SUB CONSTANT
    case 0x2A:
    {
        int32_t constant;
        int i;

        constant = (int8_t)src2;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] - constant;
        }

        updateVectorFlags(dest);

        break;
    }

    // 0x2B : VECTOR MUL CONSTANT
    case 0x2B:
    {
        int32_t constant;
        int i;

        constant = (int8_t)src2;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] * constant;
        }

        updateVectorFlags(dest);

        break;
    }

    //  0x2C : VECTOR LOAD CONSTANT ADDRESS
    case 0x2C:
    {
        int address;
        int i;

        address = src2;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = read32(proc_id, address);
            address += 4;
        }

        updateVectorFlags(dest);

        break;
    }

    //  0x2E : VECTOR STORE CONSTANT ADDRESS
    case 0x2E:
    {
        int address;
        int i;

        address = src2;
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            write32(proc_id, address, VRegister[proc_id][src1][i]);
            address += 4;
        }

        break;
    }

    // 0x2F : VECTOR ADD SCALAR REGISTER
    case 0x2F:
    {
        int32_t scalar;
        int i;

        scalar = Register[proc_id][src2];
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] + scalar;
        }

        updateVectorFlags(dest);

        break;
    }

    //  0x30 : VECTOR SUB SCALAR REGISTER
    case 0x30:
    {
        int32_t scalar;
        int i;

        scalar = Register[proc_id][src2];
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] - scalar;
        }

        updateVectorFlags(dest);

        break;
    }

    //  0x31 : VECTOR MUL SCALAR REGISTER
    case 0x31:
    {
        int32_t scalar;
        int i;

        scalar = Register[proc_id][src2];
        for (i = 0; i < VREG_ELEMENTS; i++)
        {
            VRegister[proc_id][dest][i] = VRegister[proc_id][src1][i] * scalar;
        }

        updateVectorFlags(dest);

        break;
    }

    //  INVALID OPCODE
    default:
    {
        printf("Processor %d: Invalid opcode %02X at PC=%d\n", proc_id, opcode, PC[proc_id] - 4);
        end_of_simulation[proc_id] = 1;

        break;
    }
    }
}

void processor_log_task(int processor_id, int pid, const char *program_file, const char *data_file, const char *state)
{
    if (fd_log == NULL)
        fd_log = fopen("processor.log", "a");

    if (fd_log == NULL)
        return;

    fprintf(fd_log, "\nTASK | Processor %d | PID %d | State: %s | Program: %s | Data: %s | PC: %d\n",
            processor_id, pid, state ? state : "UNKNOWN",
            program_file ? program_file : "-",
            data_file ? data_file : "-",
            (processor_id >= 0 && processor_id < NP) ? PC[processor_id] : -1);

    log_page_table(processor_id, fd_log);
    fflush(fd_log);
}

// INITIALIZE PROCESSOR
int initialize_processor(int processor_id, const char *program_file, const char *data_file)
{
    if (!valid_processor(processor_id))
        return 0;

    proc_id = processor_id;

    reset();

    if (!initialize_memory(processor_id, program_file, data_file))
    {
        end_of_simulation[processor_id] = 1;
        printf("Processor %d: memory initialization failed.\n", processor_id);
        return 0;
    }

    if (fd_log == NULL)
    {
        fd_log = fopen("processor.log", "a");
    }

    if (fd_log != NULL)
    {
        fprintf(fd_log, "\n============================================================\n");
        fprintf(fd_log, "Processor %d initialized\n", processor_id);
        fprintf(fd_log, "Program : %s\n", program_file);
        fprintf(fd_log, "Data    : %s\n", data_file);
        fprintf(fd_log, "PC      : %d\n", PC[processor_id]);
        fprintf(fd_log, "State   : READY\n");
        log_page_table(processor_id, fd_log);
        fflush(fd_log);
    }
    else
    {
        printf("Warning: cannot open processor.log\n");
    }

    printf("Processor %d initialized successfully.\n", processor_id);
    return 1;
}

//   RESET SPECIFIC PROCESSOR
void reset_processor(int processor_id)
{
    if (!valid_processor(processor_id))
        return;

    proc_id = processor_id;

    reset();
}

//   PROCESS INSTRUCTIONS
void process_instructions(int processor_id, int time_slice)
{
    int i;

    if (!valid_processor(processor_id))
        return;

    if (time_slice <= 0)
        return;

    proc_id = processor_id;

    for (i = 0; i < time_slice; i++)
    {
        if (end_of_simulation[proc_id])
            break;

        fetch();

        if (end_of_simulation[proc_id])
            break;

        decode();
        execute();
    }
    processor_sleep();
}

//   CHECK FINISHED
int processor_finished(int processor_id)
{
    if (!valid_processor(processor_id))
        return 1;

    return end_of_simulation[processor_id];
}

// FINALIZE PROCESSOR
void finalize_processor(int processor_id, const char *data_file)
{
    if (!valid_processor(processor_id))
        return;

    proc_id = processor_id;

    // Save the final mapping before physical pages are released.
    if (fd_log != NULL)
    {
        fprintf(fd_log, "\nFINAL PAGE TABLE | Processor %d\n", processor_id);
        log_page_table(processor_id, fd_log);
    }

    // The memory module writes data and then releases this task's pages.
    finalize_memory(processor_id, data_file);

    if (fd_log != NULL)
    {
        fprintf(fd_log, "Processor %d finished\n", processor_id);
        fprintf(fd_log, "Final PC : %d\n", PC[processor_id]);
        fprintf(fd_log, "State    : FINISHED\n");
        fprintf(fd_log, "============================================================\n");
        fflush(fd_log);
    }
}