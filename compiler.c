#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "compiler.h"

char programFile[256];
char bytecodeFile[256];

#define MAX_LABELS 100

typedef struct
{
    char name[64];
    int instructionNumber;
} Label;

static char *skip_spaces(char *str)
{
    while (*str == ' ' || *str == '\t')
        str++;

    return str;
}

static void remove_comment(char *line)
{
    char *p = strchr(line, '%');

    if (p != NULL)
        *p = '\0';
}

static int is_register(const char *str)
{
    return str != NULL &&
           (str[0] == 'x' || str[0] == 'X');
}

static int is_vector_register(const char *str)
{
    return str != NULL && (str[0] == 'v' || str[0] == 'V');
}

static int get_register(const char *str)
{
    if (is_register(str))
        return atoi(str + 1);

    return atoi(str);
}

static int get_vector_register(const char *str)
{
    if (is_vector_register(str))
        return atoi(str + 1);

    return -1;
}

static int is_label(const char *line)
{
    return line != NULL && line[0] == '.';
}

static int find_label(Label labels[], int count, const char *name)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].instructionNumber;
    }

    return -1;
}

static int get_branch_opcode(const char *branch)
{
    if (strcmp(branch, "BEQ") == 0)
        return 0x10;
    if (strcmp(branch, "BNE") == 0)
        return 0x11;
    if (strcmp(branch, "BCS") == 0)
        return 0x12;
    if (strcmp(branch, "BCC") == 0)
        return 0x13;
    if (strcmp(branch, "BMI") == 0)
        return 0x14;
    if (strcmp(branch, "BPL") == 0)
        return 0x15;
    if (strcmp(branch, "BVS") == 0)
        return 0x16;
    if (strcmp(branch, "BVC") == 0)
        return 0x17;
    if (strcmp(branch, "BHI") == 0)
        return 0x18;
    if (strcmp(branch, "BLS") == 0)
        return 0x19;
    if (strcmp(branch, "BGE") == 0)
        return 0x1A;
    if (strcmp(branch, "BLT") == 0)
        return 0x1B;
    if (strcmp(branch, "BGT") == 0)
        return 0x1C;
    if (strcmp(branch, "BLE") == 0)
        return 0x1D;
    if (strcmp(branch, "BAL") == 0)
        return 0x1E;

    return -1;
}

static int get_vector_opcode(char operation, int type)
{
    if (operation == '+')
    {
        if (type == 0)
            return 0x21; /* VADD  */
        if (type == 1)
            return 0x29; /* VADDI */
        if (type == 2)
            return 0x2F; /* VADDS */
    }

    if (operation == '-')
    {
        if (type == 0)
            return 0x22; /* VSUB  */
        if (type == 1)
            return 0x2A; /* VSUBI */
        if (type == 2)
            return 0x30; /* VSUBS */
    }

    if (operation == '*')
    {
        if (type == 0)
            return 0x23; /* VMUL  */
        if (type == 1)
            return 0x2B; /* VMULI */
        if (type == 2)
            return 0x31; /* VMULS */
    }

    return -1;
}

void compile(void)
{
    FILE *src;
    FILE *out;

    char line[256];
    Label labels[MAX_LABELS];

    int labelCount = 0;
    int instructionCount = 0;
    int currentInstruction = 0;

    src = fopen(programFile, "r");

    if (src == NULL)
    {
        printf("Cannot open %s\n", programFile);
        return;
    }

    // PASS 1
    while (fgets(line, sizeof(line), src) != NULL)
    {
        char temp[256];
        char *ptr;

        strcpy(temp, line);
        remove_comment(temp);
        ptr = skip_spaces(temp);

        if (*ptr == '\0' || *ptr == '\n')
            continue;

        if (is_label(ptr))
        {
            ptr[strcspn(ptr, "\r\n")] = '\0';

            if (labelCount >= MAX_LABELS)
            {
                printf("Too many labels\n");
                fclose(src);
                return;
            }

            strcpy(labels[labelCount].name, ptr);
            labels[labelCount].instructionNumber = instructionCount;
            labelCount++;
        }
        else
        {
            instructionCount++;
        }
    }

    fclose(src);

    // PASS 2
    src = fopen(programFile, "r");

    if (src == NULL)
    {
        printf("Cannot open %s\n", programFile);
        return;
    }

    out = fopen(bytecodeFile[0] ? bytecodeFile : "program.byte", "w");

    if (out == NULL)
    {
        printf("Cannot create program.byte\n");
        fclose(src);
        return;
    }

    while (fgets(line, sizeof(line), src) != NULL)
    {
        char dest[64];
        char op1[64];
        char op2[64];

        char branch[64];
        char labelName[64];

        char operation;
        int opcode;
        remove_comment(line);

        line[strcspn(line, "\r\n")] = '\0';

        {
            char *ptr = skip_spaces(line);

            if (*ptr == '\0')
                continue;

            if (is_label(ptr))
                continue;

            if (sscanf(ptr, "%63s = [%63[^]]]", dest, op1) == 2)
            {
                if (is_vector_register(dest))
                {
                    int vd = get_vector_register(dest);

                    if (is_register(op1))
                    {
                        // VLOAD
                        fprintf(out, "%02X %02X %02X %02X\n", 0x25, vd, 0, get_register(op1));
                    }
                    else
                    {
                        int address = atoi(op1);

                        fprintf(out, "%02X %02X %02X %02X\n", 0x2D, vd, 0, address & 0xFF);
                    }
                }
                else
                {
                    int rd = get_register(dest);

                    if (is_register(op1))
                    {
                        fprintf(out, "%02X %02X %02X %02X\n", 0x05, rd, 0, get_register(op1));
                    }
                    else
                    {
                        int address = atoi(op1);

                        fprintf(out, "%02X %02X %02X %02X\n", 0x0D, rd, 0, address & 0xFF);
                    }
                }

                currentInstruction++;
                continue;
            }

            // MEMORY STORE
            if (sscanf(ptr, "[%63[^]]] = %63s", op1, op2) == 2)
            {
                if (is_vector_register(op2))
                {
                    int vs = get_vector_register(op2);

                    if (is_register(op1))
                    {
                        // VSTORE
                        fprintf(out, "%02X %02X %02X %02X\n", 0x26, 0, vs, get_register(op1));
                    }
                    else
                    {
                        int address = atoi(op1);
                        fprintf(out, "%02X %02X %02X %02X\n", 0x2E, 0, vs, address & 0xFF);
                    }
                }
                else
                {
                    // Scalar store
                    if (is_register(op1) && is_register(op2))
                    {
                        fprintf(out, "%02X %02X %02X %02X\n", 0x06, get_register(op1), 0, get_register(op2));
                    }
                    else
                    {
                        int address = atoi(op1);
                        fprintf(out, "%02X %02X %02X %02X\n", 0x0E, 0, get_register(op2), address & 0xFF);
                    }
                }

                currentInstruction++;
                continue;
            }

            // BRANCH
            if (sscanf(ptr, "%63s %63s", branch, labelName) == 2)
            {
                int branchOpcode = get_branch_opcode(branch);

                if (branchOpcode != -1)
                {
                    int target;
                    int offset;

                    target = find_label(labels, labelCount, labelName);

                    if (target == -1)
                    {
                        printf("Unknown label: %s\n", labelName);
                        fclose(src);
                        fclose(out);
                        return;
                    }

                    // Branch offset is relative to the current instruction.
                    offset = target - currentInstruction;
                    fprintf(out, "%02X 00 00 %02X\n", branchOpcode, offset & 0xFF);

                    currentInstruction++;
                    continue;
                }
            }

            // ARITHMETIC
            if (sscanf(ptr, "%63s = %63s %c %63s", dest, op1, &operation, op2) == 4)
            {

                // VECTOR ARITHMETIC
                if (is_vector_register(dest))
                {
                    int vd = get_vector_register(dest);
                    int vs1 = get_vector_register(op1);

                    if (!is_vector_register(op1))
                    {
                        printf("Invalid vector expression: %s\n", ptr);

                        fclose(src);
                        fclose(out);
                        return;
                    }

                    if (is_vector_register(op2))
                    {
                        // Vector-vector
                        int vs2 = get_vector_register(op2);

                        opcode = get_vector_opcode(operation, 0);
                        if (opcode == -1)
                        {
                            printf("Invalid vector operation: %s\n", ptr);

                            fclose(src);
                            fclose(out);
                            return;
                        }

                        fprintf(out, "%02X %02X %02X %02X\n", opcode, vd, vs1, vs2);
                    }
                    else if (is_register(op2))
                    {

                        // Vector-scalar register
                        opcode = get_vector_opcode(operation, 2);

                        fprintf(out, "%02X %02X %02X %02X\n", opcode, vd, vs1, get_register(op2));
                    }
                    else
                    {
                        // Vector-immediate
                        int constant = atoi(op2);

                        if (constant < -128 || constant > 255)
                        {
                            printf("Invalid vector constant: %d\n", constant);

                            fclose(src);
                            fclose(out);
                            return;
                        }

                        opcode = get_vector_opcode(operation, 1);

                        fprintf(out, "%02X %02X %02X %02X\n", opcode, vd, vs1, constant & 0xFF);
                    }

                    currentInstruction++;
                    continue;
                }

                // SCALAR ARITHMETIC
                if (is_register(op1) && is_register(op2))
                {
                    if (operation == '+')
                        opcode = 0x01;
                    else if (operation == '-')
                        opcode = 0x02;
                    else if (operation == '*')
                        opcode = 0x03;
                    else if (operation == '/')
                        opcode = 0x04;
                    else
                        opcode = -1;

                    if (opcode != -1)
                    {
                        fprintf(out, "%02X %02X %02X %02X\n", opcode, get_register(dest), get_register(op1), get_register(op2));
                    }
                }
                else if (is_register(op1))
                {

                    // Scalar immediate arithmetic
                    int constant = atoi(op2);

                    if (operation == '+')
                        opcode = 0x09;
                    else if (operation == '-')
                        opcode = 0x0A;
                    else if (operation == '*')
                        opcode = 0x0B;
                    else if (operation == '/')
                        opcode = 0x0C;
                    else
                        opcode = -1;

                    if (opcode != -1)
                    {
                        fprintf(out, "%02X %02X %02X %02X\n", opcode, get_register(dest), get_register(op1), constant & 0xFF);
                    }
                }

                currentInstruction++;
                continue;
            }

            // MOV CONSTANT
            if (sscanf(ptr, "%63s = %63s", dest, op1) == 2)
            {
                if (is_register(dest) && !is_register(op1) && !is_vector_register(op1))
                {
                    int constant = atoi(op1);

                    if (constant < -128 || constant > 255)
                    {
                        printf("Invalid constant: %d\n", constant);

                        fclose(src);
                        fclose(out);
                        return;
                    }

                    fprintf(out, "%02X %02X %02X %02X\n", 0x0F, get_register(dest), constant & 0xFF, 0);
                    currentInstruction++;
                    continue;
                }
            }

            printf("Cannot understand line: %s\n", ptr);
        }
    }

    // END instruction
    fprintf(out, "00 00 00 00\n");

    fclose(src);
    fclose(out);

    printf("\nCOMPILATION DONE SUCCESSFULLY\n");
}

int compile_program(const char *source_file, const char *output_file)
{
    if (source_file == NULL || output_file == NULL)
        return 0;
    strncpy(programFile, source_file, sizeof(programFile) - 1);
    programFile[sizeof(programFile) - 1] = '\0';
    strncpy(bytecodeFile, output_file, sizeof(bytecodeFile) - 1);
    bytecodeFile[sizeof(bytecodeFile) - 1] = '\0';
    compile();
    return 1;
}