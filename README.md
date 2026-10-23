****# Mini-Computer System Simulation 

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
