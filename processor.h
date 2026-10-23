#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>
#include "memory.h"

#define NUM_REGISTERS 256
#define NUM_VREGISTERS 32
#define VREG_ELEMENTS 8

#define DEFAULT_TIME_SLICE 10

/* Per-processor scalar and vector registers. */
extern int32_t Register[NP][NUM_REGISTERS];
extern int32_t VRegister[NP][NUM_VREGISTERS][VREG_ELEMENTS];
   
extern int PC[NP];                                            // PROGRAM COUNTER

// Processor flags.
extern int Z[NP];
extern int N[NP];
extern int C[NP];
extern int V[NP];

extern int end_of_simulation[NP];                              // END OF SIMULATION

extern int proc_id;                                            // CURRENT PROCESSOR

// BASIC PROCESSOR FUNCTIONS
void reset(void);
void fetch(void);
void decode(void);
void execute(void);

// OS / MULTIPROCESSOR INTERFACE
int initialize_processor(int processor_id, const char *program_file, const char *data_file);
void reset_processor(int processor_id);
void process_instructions(int processor_id, int time_slice);
int processor_finished(int processor_id);
void finalize_processor(int processor_id, const char *data_file);
void processor_log_task(int processor_id, int pid, const char *program_file, const char *data_file, const char *state);

#endif