# Mini-Computer System Simulation 

## Overview

This project implements a modular mini-computer simulator in C that models the basic operations of a computer system. It simulates CPU instruction execution, register management, memory operations, and process scheduling to demonstrate how hardware and operating system concepts work together.

## Features

### 1. CPU and Instruction Cycle Simulation
- **Instruction Execution:** Simulates the execution of machine instructions through the CPU instruction cycle.
- **Register File:** Models CPU registers used to store temporary data, operands, and intermediate results.
- **Instruction Processing:** Fetches, decodes, and executes instructions to reproduce the basic working of a processor.
- **Modular Design:** Organizes the simulator into separate components to improve code readability, maintainability, and testing.

### 2. Virtual Memory Management
- **Multi-Level Page Tables:** Simulates hierarchical page tables for translating virtual addresses into physical addresses.
- **TLB (Translation Lookaside Buffer):** Implements a cache for recently used address translations to reduce page-table lookup overhead.
- **Address Translation:** Maps virtual addresses to physical memory locations using page-table entries and TLB lookups.
- **Page Replacement:** Implements a page replacement mechanism to manage physical memory when available frames are insufficient for requested pages.
- **Page Fault Handling:** Models situations where a requested page is not currently present in physical memory and needs to be loaded.

### 3. Process Scheduling
- Simulates the management and scheduling of processes competing for CPU execution.
- Coordinates process execution with the simulated CPU and memory system.

## Concepts Demonstrated

- CPU architecture and instruction execution
- Register-level data management
- Operating system process scheduling
- Virtual and physical address translation
- Multi-level page-table organization
- TLB caching and lookup optimization
- Page faults and page replacement

## Technologies Used

- **Language:** C
- **Concepts:** Computer Architecture, Operating Systems, CPU Simulation, Virtual Memory Management


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
