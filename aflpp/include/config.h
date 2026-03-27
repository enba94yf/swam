/*
   american fuzzy lop++ - vaguely configurable bits
   ------------------------------------------------

   Originally written by Michal Zalewski

   Now maintained by Marc Heuse <mh@mh-sec.de>,
                     Dominik Maier <mail@dmnk.co>
                     Andrea Fioraldi <andreafioraldi@gmail.com>,
                     Heiko Eissfeldt <heiko.eissfeldt@hexco.de>,

   Copyright 2016, 2017 Google Inc. All rights reserved.
   Copyright 2019-2024 AFLplusplus Project. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     https://www.apache.org/licenses/LICENSE-2.0

 */

#ifndef _HAVE_CONFIG_H
#define _HAVE_CONFIG_H

/* Version string: */

// c = release, a = volatile github dev, e = experimental branch
#define VERSION "++4.34c"

/* Enable SWAM-style AFL++ behaviour (queue weighting, sync tweaks, wasm
   mutator helpers, etc.).

   - When this macro is *undefined*, behaviour should match vanilla
     AFL++ v4.34c as closely as possible.
   - When this macro is *defined* (the default in this repository), additional
     SWAM helpers are enabled and afl-fuzz links the extra interesting_64 /
     interesting_f32 / interesting_f64 symbols used by the wasm mutator.

   Notes:
   - This repository enables SWAM mode by default (macro set to 1).
   - To approximate upstream AFL++ v4.34c behavior, temporarily comment out the
     macro below and rebuild both afl-fuzz and custom_mutators/wasm. */
#define AFL_SWAM 1

/******************************************************
 *                                                    *
 *  Settings that may be of interest to power users:  *
 *                                                    *
 ******************************************************/

/* Default shared memory map size. Most targets just need a coverage map
   between 20-250kb. Plus there is an auto-detection feature in afl-fuzz.
   However if a target has problematic constructors and init arrays then
   this can fail. Hence afl-fuzz deploys a larger default map. The largest
   map seen so far is the xlsx fuzzer for libreoffice which is 5MB.
   At runtime this value can be overridden via AFL_MAP_SIZE.
   Default: 8MB (defined in bytes) */
#define DEFAULT_SHMEM_SIZE (8 * 1024 * 1024)

/* Default time until when no more coverage finds are happening afl-fuzz
   switches to exploitation mode. It automatically switches back when new
   coverage is found.
   Default: 300 (seconds) */
#define STRATEGY_SWITCH_TIME 1000

/* Default file permission umode when creating directories */
#define DEFAULT_DIRS_PERMISSION 0700

/* Default file permission umode when creating files (default: 0600) */
#define DEFAULT_PERMISSION 0600

#ifdef __APPLE__
  #include <TargetConditionals.h>
  #if TARGET_OS_IOS
    #undef DEFAULT_PERMISSION
    #define DEFAULT_PERMISSION 0666
  #endif
#endif
#ifdef __ANDROID__
  #undef DEFAULT_PERMISSION
  #define DEFAULT_PERMISSION 0666
#endif

/* SkipDet's global configuration */

#define MINIMAL_BLOCK_SIZE 64
#define SMALL_DET_TIME (60 * 1000 * 1000U)
#define MAXIMUM_INF_EXECS (16 * 1024U)
#define MAXIMUM_QUICK_EFF_EXECS (64 * 1024U)
#define THRESHOLD_DEC_TIME (20 * 60 * 1000U)

/* Set the Prob of selecting eff_bytes 3 times more than original,
   Now disabled */
#define EFF_HAVOC_RATE 3

/* CMPLOG/REDQUEEN TUNING
 *
 * Here you can modify tuning and solving options for CMPLOG.
 * Note that these are run-time options for afl-fuzz, no target
 * recompilation required.
 *
 */

/* If a redqueen pass finds more than one solution, try to combine them? */
#define CMPLOG_COMBINE

/* Minimum % of the corpus to perform cmplog on. Default: 10% */
#define CMPLOG_CORPUS_PERCENT 5U

/* Number of potential positions from which we decide if cmplog becomes
   useless, default 12288 */
#define CMPLOG_POSITIONS_MAX (12 * 1024)

/* Maximum allowed fails per CMP value. Default: 96 */
#define CMPLOG_FAIL_MAX 96

/*
 * Effective fuzzing with selective feeding inputs
 */

#define MAX_EXTRA_SAN_BINARY 4

/* -------------------------------------*/
/* Now non-cmplog configuration options */
/* -------------------------------------*/

/* If a persistent target keeps state and found crashes are not reproducible
   then enable this option and set the AFL_PERSISTENT_RECORD env variable
   to a number. These number of testcases prior and including the crash case
   will be kept and written to the crash/ directory as RECORD:... files.
   Note that every crash will be written, not only unique ones! */

// #define AFL_PERSISTENT_RECORD

/* Adds support in compiler-rt to replay persistent records in @@-style
 * harnesses */

//  #define AFL_PERSISTENT_REPLAY_ARGPARSE

/* console output colors: There are three ways to configure its behavior
 * 1. default: colored outputs fixed on: defined USE_COLOR && defined
 * ALWAYS_COLORED The env var. AFL_NO_COLOR will have no effect
 * 2. defined USE_COLOR && !defined ALWAYS_COLORED
 *    -> depending on env var AFL_NO_COLOR=1 colors can be switched off
 *    at run-time. Default is to use colors.
 * 3. colored outputs fixed off: !defined USE_COLOR
 *    The env var. AFL_NO_COLOR will have no effect
 */

/* Comment out to disable terminal colors (note that this makes afl-analyze
   a lot less nice): */

#define USE_COLOR

#ifdef USE_COLOR
  /* Comment in to always enable terminal colors */
  /* Comment out to enable runtime controlled terminal colors via AFL_NO_COLOR
   */
  #define ALWAYS_COLORED 1
#endif

/* StatsD config
   Config can be adjusted via AFL_STATSD_HOST and AFL_STATSD_PORT environment
   variable.
*/
#define STATSD_UPDATE_SEC 1
#define STATSD_DEFAULT_PORT 8125
#define STATSD_DEFAULT_HOST "127.0.0.1"

/* If you want to have the original afl internal memory corruption checks.
   Disabled by default for speed. it is better to use "make ASAN_BUILD=1". */

// #define _WANT_ORIGINAL_AFL_ALLOC

/* Comment out to disable fancy boxes and use poor man's 7-bit UI: */

#ifndef DISABLE_FANCY
  #define FANCY_BOXES
#endif

/* Default timeout for fuzzed code (milliseconds). This is the upper bound,
   also used for detecting hangs; the actual value is auto-scaled: */

#define EXEC_TIMEOUT 1000U

/* Timeout rounding factor when auto-scaling (milliseconds): */

#define EXEC_TM_ROUND 20U

/* 64bit arch MACRO */
#if (defined(__x86_64__) || defined(__arm64__) || defined(__aarch64__) ||    \
     (defined(__riscv) && __riscv_xlen == 64) || defined(__powerpc64le__) || \
     defined(__s390x__) || defined(__loongarch64))
  #define WORD_SIZE_64 1
#endif

/* Default memory limit for child process (MB) 0 = disabled : */

#define MEM_LIMIT 0U

/* Default memory limit when running in QEMU mode (MB) 0 = disabled : */

#define MEM_LIMIT_QEMU 0U

/* Default memory limit when running in Unicorn mode (MB) 0 = disabled : */

#define MEM_LIMIT_UNICORN 0U

/* Number of calibration cycles per every new test case (and for test
   cases that show variable behavior): */

#define CAL_CYCLES_FAST 3U
#define CAL_CYCLES 7U
#define CAL_CYCLES_LONG 12U

/* Number of subsequent timeouts before abandoning an input file: */

#define TMOUT_LIMIT 250U

/* Maximum number of unique hangs or crashes to record: */

#ifndef AFL_SWAM
#define KEEP_UNIQUE_HANG 512U
#define KEEP_UNIQUE_CRASH 25600U
#else
#define KEEP_UNIQUE_HANG 500U
#define KEEP_UNIQUE_CRASH 10000U
#endif

/* Baseline number of random tweaks during a single 'havoc' stage: */

#define HAVOC_CYCLES 256U
#define HAVOC_CYCLES_INIT 1024U

/* Maximum multiplier for the above (should be a power of two, beware
   of 32-bit int overflows): */

#define HAVOC_MAX_MULT 64U
#define HAVOC_MAX_MULT_MOPT 64U

/* Absolute minimum number of havoc cycles (after all adjustments): */

#define HAVOC_MIN 12U

/* Power Schedule Divisor */
#define POWER_BETA 1U
#define MAX_FACTOR (POWER_BETA * 32)

/* Maximum stacking for havoc-stage tweaks. The actual value is calculated
   like this:

   n = random between 1 and HAVOC_STACK_POW2
   stacking = 2^n

   In other words, the default (n = 4) produces 2, 4, 8, 16
   stacked tweaks: */

#define HAVOC_STACK_POW2 4U

/* Caps on block sizes for cloning and deletion operations. Each of these
   ranges has a 33% probability of getting picked, except for the first
   two cycles where smaller blocks are favored: */

#define HAVOC_BLK_SMALL 32U
#define HAVOC_BLK_MEDIUM 128U
#define HAVOC_BLK_LARGE 1500U

/* Extra-large blocks, selected very rarely (<5% of the time): */

#define HAVOC_BLK_XL 32768U

/* Probabilities of skipping non-favored entries in the queue, expressed as
   percentages: */

#define SKIP_TO_NEW_PROB 99     /* ...when there are new, pending favorites */
#define SKIP_NFAV_OLD_PROB 95   /* ...no new favs, cur entry already fuzzed */
#define SKIP_NFAV_NEW_PROB 75   /* ...no new favs, cur entry not fuzzed yet */

/* Splicing cycle count: */

#define SPLICE_CYCLES 15

/* Nominal per-splice havoc cycle length: */

#define SPLICE_HAVOC 32

/* Maximum offset for integer addition / subtraction stages: */

#define ARITH_MAX 35

/* Limits for the test case trimmer. The absolute minimum chunk size; and
   the starting and ending divisors for chopping up the input file: */

#define TRIM_MIN_BYTES 4
#define TRIM_START_STEPS 16
#define TRIM_END_STEPS 1024

/* Maximum size of input file, in bytes (keep under 100MB, default 1MB):
   (note that if this value is changed, several areas in afl-cc.c, afl-fuzz.c
   and afl-fuzz-state.c have to be changed as well! */

#ifndef AFL_SWAM
#define MAX_FILE (1 * 1024 * 1024L)
#else
#define MAX_FILE (8 * 1024 * 1024L)
#endif

/* The same, for the test case minimizer: */

#define TMIN_MAX_FILE (10 * 1024 * 1024L)

/* Block normalization steps for afl-tmin: */

#define TMIN_SET_MIN_SIZE 4
#define TMIN_SET_STEPS 128

/* Maximum dictionary token size (-x), in bytes: */

#define MAX_DICT_FILE 128

/* Length limits for auto-detected dictionary tokens: */

#define MIN_AUTO_EXTRA 3
#define MAX_AUTO_EXTRA 32

/* Maximum number of user-specified dictionary tokens to use in deterministic
   steps; past this point, the "extras/user" step will be still carried out,
   but with proportionally lower odds: */

#define MAX_DET_EXTRAS 256

/* Maximum number of auto-extracted dictionary tokens to actually use in fuzzing
   (first value), and to keep in memory as candidates. The latter should be much
   higher than the former. */

#define USE_AUTO_EXTRAS 4096
#define MAX_AUTO_EXTRAS (USE_AUTO_EXTRAS * 8)

/* Scaling factor for the effector map used to skip some of the more
   expensive deterministic steps. The actual divisor is set to
   2^EFF_MAP_SCALE2 bytes: */

#define EFF_MAP_SCALE2 3

/* Minimum input file length at which the effector logic kicks in: */

#define EFF_MIN_LEN 128

/* Maximum effector density past which everything is just fuzzed
   unconditionally (%): */

#define EFF_MAX_PERC 90

/* UI refresh frequency (Hz): */

#define UI_TARGET_HZ 5

/* Fuzzer stats file, queue stats and plot update intervals (sec): */

#define STATS_UPDATE_SEC 60
#define PLOT_UPDATE_SEC 5
#define QUEUE_UPDATE_SEC 1800

/* Smoothing divisor for CPU load and exec speed stats (1 - no smoothing). */

#define AVG_SMOOTHING 16

/* Max length of sync id (the id after -M and -S) */

#define SYNC_ID_MAX_LEN 50

/* Sync interval (every n havoc cycles): */

#define SYNC_INTERVAL 8

/* Sync time (minimum time between syncing in ms, time is halfed for -M main
   nodes) - default is 20 minutes (30 minutes in SWAM mode): */

#ifndef AFL_SWAM
#define SYNC_TIME (20 * 60 * 1000)
#else
#define SYNC_TIME (30 * 60 * 1000)
#endif

/* Output directory reuse grace period (minutes): */

#define OUTPUT_GRACE 25

/* Uncomment to use simple file names (id_NNNNNN): */

// #define SIMPLE_FILES

/* List of interesting values to use in fuzzing. */

#define INTERESTING_8                                    \
  -128,    /* Overflow signed 8-bit when decremented  */ \
      -1,  /*                                         */ \
      0,   /*                                         */ \
      1,   /*                                         */ \
      16,  /* One-off with common buffer size         */ \
      32,  /* One-off with common buffer size         */ \
      64,  /* One-off with common buffer size         */ \
      100, /* One-off with common buffer size         */ \
      127                        /* Overflow signed 8-bit when incremented  */

#define INTERESTING_8_LEN 9

#define INTERESTING_16                                    \
  -32768,   /* Overflow signed 16-bit when decremented */ \
      -129, /* Overflow signed 8-bit                   */ \
      128,  /* Overflow signed 8-bit                   */ \
      255,  /* Overflow unsig 8-bit when incremented   */ \
      256,  /* Overflow unsig 8-bit                    */ \
      512,  /* One-off with common buffer size         */ \
      1000, /* One-off with common buffer size         */ \
      1024, /* One-off with common buffer size         */ \
      4096, /* One-off with common buffer size         */ \
      32767                      /* Overflow signed 16-bit when incremented */

#define INTERESTING_16_LEN 10

#ifndef AFL_SWAM

#define INTERESTING_32                                          \
  -2147483648LL,  /* Overflow signed 32-bit when decremented */ \
      -100663046, /* Large negative number (endian-agnostic) */ \
      -32769,     /* Overflow signed 16-bit                  */ \
      32768,      /* Overflow signed 16-bit                  */ \
      65535,      /* Overflow unsig 16-bit when incremented  */ \
      65536,      /* Overflow unsig 16 bit                   */ \
      100663045,  /* Large positive number (endian-agnostic) */ \
      2139095040, /* float infinite                          */ \
      2147483647                 /* Overflow signed 32-bit when incremented */

#define INTERESTING_32_LEN 9

#else /* AFL_SWAM */

#define INTERESTING_32                                          \
  -2147483648LL,  /* Overflow signed 32-bit when decremented */ \
      -100663046, /* Large negative number (endian-agnostic) */ \
      -32769,     /* Overflow signed 16-bit                  */ \
      32768,      /* Overflow signed 16-bit                  */ \
      65535,      /* Overflow unsig 16-bit when incremented  */ \
      65536,      /* Overflow unsig 16 bit                   */ \
      100663045,  /* Large positive number (endian-agnostic) */ \
      2147483647                 /* Overflow signed 32-bit when incremented */

#define INTERESTING_32_LEN 8

#define INTERESTING_64                                                     \
  -1-9223372036854775807LL,  /* Overflow signed 64-bit when decremented */ \
      -432345564227567366LL, /* Large negative number (endian-agnostic) */ \
      -2147483649,           /* Overflow signed 32-bit                  */ \
      2147483648,            /* Overflow signed 32-bit                  */ \
      4294967295,            /* Overflow unsig 32-bit when incremented  */ \
      4294967296,            /* Overflow unsig 32-bit                   */ \
      432345564227567365LL,  /* Large positive number (endian-agnostic) */ \
      9223372036854775807LL      /* Overflow signed 64-bit when incremented */

#define INTERESTING_64_LEN 8

#define INTERESTING_F32                                                  \
      0x00000000,          /* +0.0                                    */ \
      0x80000000,          /* -0.0                                    */ \
      0x7fc00000,          /* NaN                                     */ \
      0x7f800000,          /* Inf                                     */ \
      0xff800000,          /* -Inf                                    */ \
      0x00000001,          /* +1.40129846432e-45                      */ \
      0x7f7fffff,          /* +3.40282346639e+38                      */ \
      0x80000001,          /* -1.40129846432e-45                      */ \
      0xff7fffff           /* -3.40282346639e+38                      */


#define INTERESTING_F32_LEN 9

#define INTERESTING_F64                                                 \
      0x0000000000000000,  /* +0.0                                   */ \
      0x8000000000000000,  /* -0.0                                   */ \
      0x7ff8000000000000,  /* NaN                                    */ \
      0x7ff0000000000000,  /* Inf                                    */ \
      0xfff0000000000000,  /* -Inf                                   */ \
      0x0010000000000000,  /* +2.2250738585072014e-308               */ \
      0x8010000000000000,  /* -2.2250738585072014e-308               */ \
      0x7fefffffffffffff,  /* +1.7976931348623157e+308               */ \
      0xffefffffffffffff   /* -1.7976931348623157e+308               */

#define INTERESTING_F64_LEN 9

#endif /* AFL_SWAM */

/* ----------------------------------------------------------------- */
/* Bitmap layout                                                     */

/* Index of the first slot in the coverage bitmap that is used for
   regular instrumentation.
   - In upstream AFL++ this is 4 (slots 0-3 are reserved).
   - In AFL_SWAM mode this is 5 to match the SWAM bitmap layout. */
#ifdef AFL_SWAM
#  define AFL_BITMAP_FIRST_LOC 5
#else
#  define AFL_BITMAP_FIRST_LOC 4
#endif

/***********************************************************
 *                                                         *
 *  Really exotic stuff you probably don't want to touch:  *
 *                                                         *
 ***********************************************************/

/* Call count interval between reseeding the PRNG from /dev/urandom: */

#define RESEED_RNG 2500000

/* The default maximum testcase cache size in MB, 0 = disable.
   A value between 50 and 250 is a good default value. Note that the
   number of entries will be auto assigned if not specified via the
   AFL_TESTCACHE_ENTRIES env variable */

#define TESTCASE_CACHE_SIZE 50

/* Maximum line length passed from GCC to 'as' and used for parsing
   configuration files: */

#define MAX_LINE 8192

/* Environment variable used to pass SHM ID to the called program. */

#define SHM_ENV_VAR "__AFL_SHM_ID"

/* Environment variable used to pass shared memory fuzz map id
and the mapping size to the called program. */

#define SHM_FUZZ_ENV_VAR "__AFL_SHM_FUZZ_ID"
#define SHM_FUZZ_MAP_SIZE_ENV_VAR "__AFL_SHM_FUZZ_MAP_SIZE"

/* Default size of the shared memory fuzz map.
We add 4 byte for one u32 length field. */
#define SHM_FUZZ_MAP_SIZE_DEFAULT (MAX_FILE + 4)

/* Other less interesting, internal-only variables. */

#define CLANG_ENV_VAR "__AFL_CLANG_MODE"
#define AS_LOOP_ENV_VAR "__AFL_AS_LOOPCHECK"
#define PERSIST_ENV_VAR "__AFL_PERSISTENT"
#define DEFER_ENV_VAR "__AFL_DEFER_FORKSRV"

/* In-code signatures for deferred and persistent mode. */

#define PERSIST_SIG "##SIG_AFL_PERSISTENT##"
#define DEFER_SIG "##SIG_AFL_DEFER_FORKSRV##"

/* Distinctive bitmap signature used to indicate failed execution: */

#define EXEC_FAIL_SIG 0xfee1dead

/* Distinctive exit code used to indicate MSAN trip condition: */

#define MSAN_ERROR 86

/* Distinctive exit code used to indicate LSAN trip condition: */

#define LSAN_ERROR 23

/* Designated file descriptors for forkserver commands (the application will
   use FORKSRV_FD and FORKSRV_FD + 1): */

#define FORKSRV_FD 198

/* Fork server init timeout multiplier: we'll wait the user-selected
   timeout plus this much for the fork server to spin up. */

#define FORK_WAIT_MULT 10

/* Calibration timeout adjustments, to be a bit more generous when resuming
   fuzzing sessions or trying to calibrate already-added internal finds.
   The first value is a percentage, the other is in milliseconds: */

#define CAL_TMOUT_PERC 125
#define CAL_TMOUT_ADD 50

/* Number of chances to calibrate a case before giving up: */

#define CAL_CHANCES 3

/* Map size for the traced binary (2^MAP_SIZE_POW2). Must be greater than
   2; you probably want to keep it under 18 or so for performance reasons
   (adjusting AFL_INST_RATIO when compiling is probably a better way to solve
   problems with complex programs). You need to recompile the target binary
   after changing this - otherwise, SEGVs may ensue. */

#define MAP_SIZE_POW2 16

/* Do not change this unless you really know what you are doing. */

#define MAP_SIZE (1U << MAP_SIZE_POW2)
#if MAP_SIZE <= 2097152
  #define MAP_INITIAL_SIZE (2 << 20)  // = 2097152
#else
  #define MAP_INITIAL_SIZE MAP_SIZE
#endif

/* IJON max tracking map configuration */

/* Number of IJON slots (power-of-2 for efficient bitmasking) */
#define MAP_SIZE_IJON_ENTRIES 512

/* IJON map size for set/inc/xor */
#define MAP_SIZE_IJON_MAP 65536

/* IJON map footprint in bytes (64-bit values for legacy compatibility) */
#define MAP_SIZE_IJON_BYTES (MAP_SIZE_IJON_ENTRIES * sizeof(u64))  // = 4096

/* Maximum allocator request size (keep well under INT_MAX): */

#define MAX_ALLOC 0x40000000

/* A made-up hashing seed: */

#define HASH_CONST 0xa5b35705

/* Constants for afl-gotcpu to control busy loop timing: */

#define CTEST_TARGET_MS 5000
#define CTEST_CORE_TRG_MS 1000
#define CTEST_BUSY_CYCLES (10 * 1000 * 1000)

/* Enable NeverZero counters in QEMU mode */

#define AFL_QEMU_NOT_ZERO

/* AFL RedQueen */

#define CMPLOG_SHM_ENV_VAR "__AFL_CMPLOG_SHM_ID"

/* ASAN SHM ID */
#define AFL_ASAN_FUZZ_SHM_ENV_VAR "__AFL_ASAN_SHM_ID"

/* CPU Affinity lockfile env var */

#define CPU_AFFINITY_ENV_VAR "__AFL_LOCKFILE"

/* Uncomment this to use inferior block-coverage-based instrumentation. Note
   that you need to recompile the target binary for this to have any effect: */

// #define COVERAGE_ONLY

/* Uncomment this to ignore hit counts and output just one bit per tuple.
   As with the previous setting, you will need to recompile the target
   binary: */

// #define SKIP_COUNTS

/* Uncomment this to use instrumentation data to record newly discovered paths,
   but do not use them as seeds for fuzzing. This is useful for conveniently
   measuring coverage that could be attained by a "dumb" fuzzing algorithm: */

// #define IGNORE_FINDS

/* Text mutations */

/* Minimum length of a queue input to be evaluated for "is_ascii"? */

#define AFL_TXT_MIN_LEN 12

/* Maximum length of a queue input to be evaluated for "is_ascii"? */

#define AFL_TXT_MAX_LEN 65535

/* What is the minimum percentage of ascii characters present to be classified
   as "is_ascii"? */

#define AFL_TXT_MIN_PERCENT 99

/* How often to perform ASCII mutations 0 = disable, 1-8 are good values */

#define AFL_TXT_BIAS 6

/* Maximum length of a string to tamper with */

#define AFL_TXT_STRING_MAX_LEN 1024

/* Maximum mutations on a string */

#define AFL_TXT_STRING_MAX_MUTATIONS 6

#endif                                                  /* ! _HAVE_CONFIG_H */
