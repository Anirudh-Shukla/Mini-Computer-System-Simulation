# Lab 5 Test Cases

Each test contains `program.txt`, `program.byte`, `data.byte`, and `expected.txt`.
The expected files record the values used to verify correctness after execution.

| Test | Purpose | Expected result |
|---|---|---|
| array_add | Scalar array addition | Sum = 2160 at address 20 |
| array_add_vector | 16-element vector addition | 16 values of 17 at address 132 |
| fir_filter | Scalar FIR calculation | 8 at address 16 |
| fir_filter_vector | 8-lane vector FIR | 5, 10, 15, 20, 25, 30, 35, 40 at address 132 |

All four tests were executed successfully with the Lab 5 scheduler and paging system.
The vector array-add test avoids the signed 8-bit immediate limitation by computing address 132 with register arithmetic.
