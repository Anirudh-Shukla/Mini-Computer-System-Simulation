# LAB 5 - Mini Computer Simulator

This submission continues the Lab 4 mini-computer simulator and adds the Lab 5 memory-management/paging requirement without removing the existing vector processor, four-processor OS, scheduler, shell, compiler, or test programs.

## Lab 5 addition

- Physical memory: 8192 bytes.
- Page size: 512 bytes.
- Physical pages: 16.
- Each process gets its own page table.
- Logical instruction memory is 256 bytes.
- Logical data memory is 4096 bytes.
- The OS/memory layer allocates only the physical pages required by each loaded program and data file.
- Processor instruction fetches and data reads/writes translate logical addresses to physical addresses.
- Pages are released when a task finishes.

The existing Lab 4 vector instructions, four processors, round-robin scheduling, PID mapping, waiting queue, shell, compiler, and test cases are retained.

## Build

```bash
make clean
make
```

## Run

```bash
./minicomputer.exe
```

Inside the shell:

```text
$ submit tests/array_add/program.txt tests/array_add/data.byte
$ submit tests/fir_filter/program.txt tests/fir_filter/data.byte
$ submit tests/array_add_vector/program.txt tests/array_add_vector/data.byte
$ submit tests/fir_filter_vector/program.txt tests/fir_filter_vector/data.byte
$ status
$ run
```

Use `exit` to leave the shell after submitted tasks have been scheduled as desired.
