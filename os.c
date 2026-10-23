#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "os.h"
#include "compiler.h"
#include "processor.h"

#define NP 4
#define MAX_TASKS 64
#define TIME_SLICE 10
#define COMMAND_SIZE 512

typedef enum
{
    TASK_UNUSED,
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_FINISHED
} TaskState;

typedef struct
{
    int pid;
    int processor_id;
    TaskState state;
    char program_file[256];
    char bytecode_file[256];
    char data_file[256];
} Task;

static Task tasks[MAX_TASKS];
static int processor_pid[NP];
static int next_pid = 1;
static int os_running = 1;

static void reset_tasks(void)
{
    int i;
    for (i = 0; i < MAX_TASKS; i++)
    {
        tasks[i].pid = -1;
        tasks[i].processor_id = -1;
        tasks[i].state = TASK_UNUSED;
        tasks[i].program_file[0] = tasks[i].bytecode_file[0] = tasks[i].data_file[0] = '\0';
    }
    for (i = 0; i < NP; i++)
        processor_pid[i] = -1;
    next_pid = 1;
}

static int free_slot(void)
{
    int i;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_FINISHED)
            return i;
    return -1;
}
static int free_processor(void)
{
    int i;
    for (i = 0; i < NP; i++)
        if (processor_pid[i] < 0)
            return i;
    return -1;
}
static int task_slot(int pid)
{
    int i;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].pid == pid && tasks[i].state != TASK_UNUSED)
            return i;
    return -1;
}
static int active_tasks(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING || tasks[i].state == TASK_WAITING)
            n++;
    return n;
}

static void make_bytecode_name(const char *program, char *out, size_t n)
{
    const char *slash1 = strrchr(program, '/');
    const char *slash2 = strrchr(program, '\\');
    const char *slash = slash1 > slash2 ? slash1 : slash2;
    size_t len = strlen(program);
    if (len >= 4 && strcmp(program + len - 4, ".txt") == 0)
        len -= 4;
    if (slash)
    {
        size_t prefix = (size_t)(slash - program) + 1;
        if (prefix >= n)
            prefix = n - 1;
        memcpy(out, program, prefix);
        out[prefix] = '\0';
        strncat(out, "program.byte", n - strlen(out) - 1);
    }
    else
    {
        snprintf(out, n, "program.byte");
    }
}

static int load_task(const char *program, const char *bytecode, const char *data)
{
    int slot = free_slot(), p = free_processor(), pid;
    if (slot < 0)
    {
        printf("OS: Maximum task limit reached.\n");
        return -1;
    }
    pid = next_pid++;
    tasks[slot].pid = pid;
    strncpy(tasks[slot].program_file, program, 255);
    tasks[slot].program_file[255] = '\0';
    strncpy(tasks[slot].bytecode_file, bytecode, 255);
    tasks[slot].bytecode_file[255] = '\0';
    strncpy(tasks[slot].data_file, data, 255);
    tasks[slot].data_file[255] = '\0';
    if (p < 0)
    {
        tasks[slot].processor_id = -1;
        tasks[slot].state = TASK_WAITING;
        printf("OS: All processors busy. PID %d placed in waiting queue.\n", pid);
        return pid;
    }
    /* A processor is selected only provisionally. Lab 5 page allocation can
       fail even when a processor is free, so keep the task waiting if its
       instruction/data pages cannot currently fit in physical memory. */
    if (!initialize_processor(p, bytecode, data))
    {
        tasks[slot].processor_id = -1;
        tasks[slot].state = TASK_WAITING;
        printf("OS: Physical memory unavailable. PID %d placed in waiting queue.\n", pid);
        return pid;
    }

    tasks[slot].processor_id = p;
    tasks[slot].state = TASK_READY;
    processor_pid[p] = pid;
    reset_processor(p);
    processor_log_task(p, pid, program, data, "READY");
    printf("OS: Loaded PID %d on Processor %d\n", pid, p);
    return pid;
}

static void assign_waiting(void)
{
    int i, p;
    while ((p = free_processor()) >= 0)
    {
        int found = -1;
        for (i = 0; i < MAX_TASKS; i++)
            if (tasks[i].state == TASK_WAITING)
            {
                found = i;
                break;
            }
        if (found < 0)
            break;
        if (!initialize_processor(p, tasks[found].bytecode_file, tasks[found].data_file))
        {
            /* Physical pages are still unavailable. Keep the task waiting and
               stop trying this free processor until another task finishes. */
            break;
        }
        tasks[found].processor_id = p;
        tasks[found].state = TASK_READY;
        processor_pid[p] = tasks[found].pid;
        reset_processor(p);
        processor_log_task(p, tasks[found].pid, tasks[found].program_file, tasks[found].data_file, "READY");
        printf("OS: PID %d moved from waiting queue to Processor %d\n", tasks[found].pid, p);
    }
}

static void finish_task(int pid)
{
    int s = task_slot(pid);
    if (s < 0)
        return;
    int p = tasks[s].processor_id;
    if (p >= 0 && p < NP)
    {
        finalize_processor(p, tasks[s].data_file);
        processor_pid[p] = -1;
    }
    tasks[s].processor_id = -1;
    tasks[s].state = TASK_FINISHED;
    processor_log_task(p, pid, tasks[s].program_file, tasks[s].data_file, "FINISHED");
    printf("OS: PID %d finished on Processor %d.\n", pid, p);
    assign_waiting();
}

static void run_one(int pid)
{
    int s = task_slot(pid);
    if (s < 0)
        return;
    int p = tasks[s].processor_id;
    if (p < 0 || p >= NP)
        return;
    tasks[s].state = TASK_RUNNING;
    processor_log_task(p, pid, tasks[s].program_file, tasks[s].data_file, "RUNNING");
    process_instructions(p, TIME_SLICE);
    if (processor_finished(p))
        finish_task(pid);
    else
        tasks[s].state = TASK_READY;
}

static void show_status(void)
{
    int i;
    printf("\n--- PROCESSOR STATUS ---\n");
    for (i = 0; i < NP; i++)
    {
        if (processor_pid[i] < 0)
            printf("Processor %d : FREE\n", i);
        else
        {
            int s = task_slot(processor_pid[i]);
            printf("Processor %d : PID %d (%s)\n", i, processor_pid[i], s >= 0 ? (tasks[s].state == TASK_RUNNING ? "RUNNING" : "READY") : "UNKNOWN");
        }
    }
    printf("Waiting queue: ");
    {
        int any = 0;
        for (i = 0; i < MAX_TASKS; i++)
            if (tasks[i].state == TASK_WAITING)
            {
                printf("PID %d ", tasks[i].pid);
                any = 1;
            }
        if (!any)
            printf("empty");
    }
    printf("\n-------------------------\n");
}

static void scheduler(void)
{
    int i;
    printf("\nOS: Scheduler started.\n");
    while (active_tasks() > 0)
    {
        int ran = 0;
        for (i = 0; i < MAX_TASKS; i++)
        {
            if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING)
            {
                ran = 1;
                run_one(tasks[i].pid);
            }
        }
        assign_waiting();
        if (!ran && active_tasks() > 0)
            break;
    }
    printf("OS: Scheduler completed.\n");
}

static void command_submit(char *args)
{
    char program[256], data[256], bytecode[256];
    if (sscanf(args, "%255s %255s", program, data) < 1)
    {
        printf("Usage: submit <program.txt> <data.byte>\n");
        return;
    }
    if (data[0] == '\0')
        strcpy(data, "data.byte");
    make_bytecode_name(program, bytecode, sizeof(bytecode));
    printf("\nOS: New task: %s\n", program);
    if (!compile_program(program, bytecode))
    {
        printf("OS: Compilation failed.\n");
        return;
    }
    load_task(program, bytecode, data);
}

static void shell(void)
{
    char command[COMMAND_SIZE];
    while (os_running)
    {
        printf("\n$ ");
        fflush(stdout);
        if (!fgets(command, sizeof(command), stdin))
            break;
        command[strcspn(command, "\r\n")] = '\0';
        if (command[0] == '\0')
            continue;
        if (strcmp(command, "exit") == 0)
        {
            os_running = 0;
            break;
        }
        if (strcmp(command, "status") == 0)
        {
            show_status();
            continue;
        }
        if (strcmp(command, "run") == 0)
        {
            scheduler();
            continue;
        }
        if (strcmp(command, "help") == 0)
        {
            printf("submit <program.txt> <data.byte>  Load a task\nrun                              Run all queued tasks\nstatus                           Show processor/task status\nexit                             Exit\n");
            continue;
        }
        if (strncmp(command, "submit ", 7) == 0)
        {
            command_submit(command + 7);
            continue;
        }
        /* Also accept a plain program path for convenience. */
        command_submit(command);
    }
}

void os_init(void)
{
    memory_system_init();
    reset_tasks();
    os_running = 1;
    printf("\n========================================\n");
    printf("          MINI COMPUTER OS\n");
    printf("========================================\n");
    printf("Processors : %d\nTime Slice : %d instructions\n", NP, TIME_SLICE);
    printf("Commands: submit, run, status, help, exit\n");
}

void os_run(void)
{
    shell();
}

void os_shutdown(void)
{
    printf("\nOS: Shutdown complete.\n");
}

void os_start(void)
{
    os_init();
    os_run();
    os_shutdown();
}
