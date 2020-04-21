# CacheQuery
[![DOI](https://zenodo.org/badge/227137498.svg)](https://zenodo.org/badge/latestdoi/227137498)

A tool for interacting with hardware memory caches in modern Intel CPUs.

* Linux Kernel module: generate non-interfering x86 code of arbitrary memory access sequences automatically profiled.
* Low noise environment: disable hardware prefetchers, hyperthreading, frequency scaling, etc.
* Support for TSC, core cycle (default), and performance counters (L3, L2, and L1, misses) (see `config/settings.h` or `/sys/kernel/cachequery/config/[use_pmc|core_cycles]/val` booleans).
* Sysfs at `/sys/kernel/cachequery/<level>/<set>/run` accepts queries of logical blocks produced by the fronted and returns sequence of hits and misses for the target cache set and level. Note that `<set>` is `((index << slice_bits) | slice)`.
* `tool/cachequery.py` provides a high-level interface with a REPL environment.


# Run

The following command runs a single MemBlockLang (MBL) query against L3's set 33:

```
$ cd tool/
$ ./cachequery.py -l l3 -s 33 @ M _?
(L3:33) r @ M _?
0 1 2 3 4 5 6 7 8 9 10 11 12 0? -> 0
0 1 2 3 4 5 6 7 8 9 10 11 12 1? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 2? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 3? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 4? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 5? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 6? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 7? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 8? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 9? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 10? -> 100
0 1 2 3 4 5 6 7 8 9 10 11 12 11? -> 100
```

Example of a 12-ways L3 cache set, where the LRU block is evicted by `M`. Output value is the number of measured HITs (change number of repetitions as you like in `config/settings.h` or `/sys/kernel/cachequery/config/num_repetitions/val`).


# Install

Tested on Linux kernel >= 4.9.x branches.

Modify `config/settings.h` as required and select the specific architecture. Some settings can be dynamically modified later on via `/sys/kernel/cachequery/config/`.

If no timing thresholds are given it will automatically compute some, but calibration takes time and is done on each execution.

(WARNING: The code is unstable and it can crash your system. Use it under your own risk.)

```
$ make cpu=iX-yyyy
$ make install
```

Current support for `i7-4790`, `i5-6500` (default), and `i7-8550u`. Add header file in `config/` and build with corresponding `make cpu=iX-yyyy`.

### Create new config file

The main parameters required for building a new config file are the cache associativity (`L?_CACHE_WAYS`), the number of set index bits (`L?_SET_BITS`), and the number of bits used for slicing (`L?_SLICE_BITS`). The associativity (or ways) and number of cache sets can be obtained with the `cpuid` command, although the sets need to be divided by the number of slices. Slices are not documented and might require manual inference, but for post-Skylake Intel machines seems to be `8`.

We recommend the default values (copy an existent file) for everything else, and manually tune them if required.

Initially we recommend to use the automatic calibration for the thresholds, perform some test runs, and check the computed threshold from the system logs. Once we are confident with the threshold, we can set it statically in the config file or dynamically via the virtual file system.


## Dependencies

Lark parser: `pip3 install lark-parser`

LevelDB + Plyvel: https://plyvel.readthedocs.io/en/latest/installation.html


## Help:

```
$ ./cachequery.py -h

    [!] ./cachequery [options] <query>

    Options:
        -h --help
        -i --interactive
        -v --verbose

        -c --config=filename    path to filename with config (default: 'cachequery.ini')
        -b --batch              path to filename with list of commands
        -o --output             path to output file for session log

        -l --level              target cache level: L3|L2|L1
        -s --set                target cache set number
```

By default it loads `tool/cachequery.ini` configuration file.


Current support for `i7-4790`, `i5-6500` (default), and `i7-8550u`. Add header file in `config/` and build with corresponding `make cpu=iX-yyyy`.

## Uninstall
```
$ make uninstall
```

# MemBlockLang

Simple language to facilitate manual writing of cache queries.

A query is a sequence of one or more memory operations. Each memory operation is specified as a *block* (represented by arbitary identifiers), and it is decorated with an optional *tag* (`?` for profiling, or `!` for flushing, no tag means just access).

MBL features several macros:

* *Expansion* macro `@`, that produces a sequence of associativity many different blocks in increasing orders. For example, for associativity 8, `@` expands to `a b c d e f g h`.
* A *wildcard* macro `_`, that produces associativity many different queries, each one consisting of a different block. For example, for associativity 8, `_` expands to the set of single-block queries `a, b, c, d, e, f, g, h`.
* Concatenation of queries is implicit.
* An *extension* macro, `s1 [s2]` that takes as input queries `s1` and `s2` and creates `|s2|` copies of `s1` extending each of them with a different element of `s2`. For example, `(a b c d)[e f]` expands to `a b c d e, a b c d f`.
* A *power* operator, `(s1)N` that repeats a query `n` times. For example, `(a b c)3` expands to `a b c a b c a b c`.
* A tag over `(s1)` or `[s1]` applies to every block. For example, `(a b)?` expands to `a? b?`.

Extensions:

* A single `!` without a preceding block executes `wbinvd`.

# Reduce system's noise

Install `msr-tool` and `acpi-cpufreq` and load the modules with `modprobe`.

Set options to `True` in `tool/cachequery.ini` to load modules and enable/disable noise by default.

## Disable multi-core and hyperthreading

Disable: `echo 0 | sudo tee /sys/devices/system/cpu/cpu*/online`

Enable: `echo 1 | sudo tee /sys/devices/system/cpu/cpu*/online`

## HW prefetching

Disable: `wrmsr -a 0x1a4 15`

Enable: `wrmsr -a 0x1a4 0`

## Turbo Boost

Disable: `wrmsr -a 0x1a0 0x4000850089`

Enable: `wrmsr -a 0x1a0 0x850089`

## Disable frequency scaling

Recommended when using RDTSC.

Disable: `sudo cpupower frequency-set -d 2000MHz; sudo cpupower frequency-set -u 2000MHz`

Enable: `sudo cpupower frequency-set -d 1Mhz; sudo cpupower frequency-set -u 5000MHz` (use hw default limits)

## CAT of L3 cache sets

Reduce to assoc 4: `wrmsr -a 0xc90 0x000f`

Restore to assoc 16: `wrmsr -a 0xc90 0xffff`
