# Just-in-time Compilation in SQLite using LibJIT

## Building

The provided build script is `build-842-project.sh`.

Executing `./build-842-project.sh` creates a directory called `842-build`
outside of the current working directory. This directory contains two SQLite
binaries, `sqlite3-jit` and `sqlite3-no-jit`. Note that the LibJIT library is
already bundled as an archive file for `x86-64` (see `libjit.a`) and does not
need to be separately installed.

## Running

Either sqlite shells can be started by running `./sqlite3-jit` or
`./sqlite3-no-jit`. Note that `sqlite3-jit` only supports arithmetic queries
such as `SELECT 3*5+1`.

Here is an example run of `sqlite3-jit`:

```
> ./sqlite3-jit
SQLite version 3.40.0 2022-11-14 17:48:26
Enter ".help" for usage hints.
Connected to a transient in-memory database.
Use ".open FILENAME" to reopen on a persistent database.
sqlite> SELECT 3*5+1;
<Benchmarks> JIT compile time took 0.000027 seconds
<Benchmarks> JIT execution took 0.000001 seconds
16
<Benchmarks> Non-JIT execution took 0.000001 seconds
```

The provided benchmarking script is `run-842-benchmarks.py`. This script
generates random arithmetic SQL queries, runs them through both SQLite
implementations and returns benchmark numbers. The script takes as a single
argument the number of operands in the generated SQL query. For example,
`python3 run-842-benchmarks.py 5` generates an arithmetic SQL query with 5
operands and executes it on both the JIT and non-JIT implementations of SQLite
and returns benchmark results. Note that the script requires `python >= 3.5` to
work.

Here is an example benchmarking run:

```
> python3 run-842-benchmarks.py 10
Executing the following SQL with 10 operands:
SELECT 1092700975.1683087+481038088.2417464*2126821522+1778006292*975362177.6351358*1147378253-675670012*810802807*682588637.408953*1842054462

=== JIT benchmarks ===
<Benchmarks> JIT compile time took 0.000076 seconds
<Benchmarks> JIT execution took 0.000003 seconds
<Benchmarks> Non-JIT execution took 0.000001 seconds
Total execution time: 0.000004
=== non-JIT benchmarks ===
<Benchmarks> Non-JIT execution took 0.000010 seconds
<Benchmarks> Non-JIT execution took 0.000002 seconds
Total execution time: 0.000016

```

To see the LibJIT IR or assembler for the JIT compiled code, uncomment lines 270
and 285 in `src/vdbe-jit.c` and rebuild the project. Subsequent execution of
queries will print the IR/assembler for that query.
