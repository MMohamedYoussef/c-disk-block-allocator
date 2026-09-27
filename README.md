# Disk block allocator simulator (C11)

Explore how contiguous, first-fit allocation fragments a fixed-size disk. `fixtures/operations.txt` is a synthetic eight-block workload; `examples/report.txt` is its deterministic block map and statistics.

```sh
cc -std=c11 -O2 -Wall -Wextra -pedantic main.c -o allocator
./allocator 8 fixtures/operations.txt
cc -std=c11 -O2 -Wall -Wextra -pedantic -Wno-unused-function tests.c -o tests
./tests
```

Each log line is `ALLOC name count`, `FREE name`, `MAP`, or `STATS`; blank lines and `#` comments are ignored. Names use letters, digits, or underscore (at most 31 bytes). Block counts and disk capacity must be positive integers; capacity is at most 1,000,000 blocks. At most 128 files can be active. A request needs one contiguous free run. Duplicate names, unknown frees, embedded NUL bytes, malformed lines, and requests that do not fit stop with a nonzero exit and line diagnostic. The report is staged until the full log validates, so invalid logs emit no partial report. `#` in a map means occupied; `.` means free. Stats count free runs and the longest free run, which exposes external fragmentation.

Sample final line: `STATS used=4 free=4 largest_run=3 free_runs=2`.
