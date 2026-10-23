#ifndef COMPILER_H
#define COMPILER_H

extern char programFile[];
extern char bytecodeFile[];

void compile(void);
int compile_program(const char *source_file, const char *output_file);

#endif
