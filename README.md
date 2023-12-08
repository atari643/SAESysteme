# A Simple Test Framework

A simple test framework for language C, inspired by [Google Test](https://github.com/google/googletest).

The main features are:

* test framework for C/C++ projects
* a simple framework written in C, with few files to include in your project
* framework developed on Linux system based on POSIX standard, as far as possible
* easy way to integrate your tests, just by adding some *test_\*()* functions
* support test with *argv* arguments
* all tests are executed sequentially (one by one)
* different execution modes: sequential fork, parallel fork or no fork

## Compilation

Our test framework is made up of two parts:

* *libtestfw.a*: a library with all the basic routines to discover, register and run tests (see API in [testfw.h](testfw.h)).
* *libtestfw_main*: a *main()* routine to launch tests easily (optional).

You can compile it by hand quite easily.

```bash
gcc -std=c99 -Wall -g   -c -o testfw.o testfw.c
ar rcs libtestfw.a testfw.o
gcc -std=c99 -Wall -g   -c -o testfw_main.o testfw_main.c
```

Or if you prefer, you can use the CMake build system ([CMakeLists.txt](CMakeLists.txt)).

```bash
mkdir build ; cd build
cmake .. && make
```

## Writing a First Test

Adding a test *hello* in a suite *test* (the default one) is really simple, you just need to write a function *test_hello()*
with the following signature ([hello.c](hello.c)).

```c
#include <stdio.h>
#include <stdlib.h>

int test_hello(int argc, char* argv[])
{
    printf("hello world\n");
    return EXIT_SUCCESS;
}
```

A success test should always return EXIT_SUCCESS. All other cases are considered as a failure. More precisely, running a test returns one of the following status:

* SUCCESS: return EXIT_SUCCESS or 0 (normal exit)
* FAILURE: return EXIT_FAILURE or 1 (or any value different of EXIT_SUCCESS)
* KILLED: killed by any signal (SIGSEGV, SIGABRT, ...)
* TIMEOUT: after a time limit, return an exit status of 124 (following the convention used in *timeout* command)

Compile it and run it.

```bash
$ gcc -std=c99 -Wall -g -c hello.c
$ gcc hello.o testfw_main.o -o hello -rdynamic -ltestfw -ldl -L.
$ ./hello
hello world
[SUCCESS] run test "test.hello" in 0.52 ms (status 0, wstatus 0)
=> 100% tests passed, 0 tests failed out of 1
```

And that's all!

## Running Tests

Let's consider the code [sample.c](sample.c). To run all this tests, you need first to compile it and then to link it against our both library and main routine.

```bash
gcc -std=c99 -Wall -g -c sample.c
gcc sample.o testfw_main.o -o sample -rdynamic -ltestfw -ldl -L.
```

The '-rdynamic' option is required to load all symbols in the dynamic symbol table (ELF linker).
> gcc man page : Pass the flag -export-dynamic to the ELF linker, on targets that support it. This instructs the linker to add all symbols, not only used ones, to the dynamic symbol table. This option is needed for some uses of "dlopen" or to allow obtaining backtraces from within a program.

### Usage

Then, launching the main routine provide you some helpful commands to run your tests. Usage:

```text
Simple Test Framework (version 0.2)
Usage: ./sample [options] [actions] [-- <testargs> ...]
Register Options:
  -r <suite.name>: register a function "suite_name()" as a test
  -R <suite>: register all functions "suite_*()" as a test suite
Actions:
  -x: execute all registered tests (default action) [TODO: to be implemented]
  -l: list all registered tests
Execution Options:
  -m <mode>: set execution mode: "forks"|"forkp"|"nofork" [default "forks"]
Other Options:
  -o <logfile>: redirect test output to a log file [TODO: to be implemented]
  -O: redirect test stdout & stderr to /dev/null
  -t <timeout>: set time limits for each test (in sec.) [default 2] [TODO: to be implemented]
  -T: no timeout [TODO: to be implemented]
  -s: silent mode (framework only)
  -S: full silent mode (both framework and test output)
  -h: print this help message
```

### List registered tests

List all available tests in the default suite (named *test*):

```bash
$ ./sample -l
test.alarm
test.args
test.assert
test.failure
test.infiniteloop
test.segfault
test.sleep
test.success
```

To use another suite, use '-r/-R' options:

```bash
$ ./sample -R othertest -l
othertest.failure
othertest.success
```

### Run a test suite

Run your tests with some options (timeout = 2 seconds, log file = /dev/null).  By default, these tests are launched sequentially (one by one) in a forked process (mode *forks*).

```bash
$ ./sample -t 2 -O -x
[KILLED] run test "test.alarm" in 1000.56 ms (signal "Alarm clock")
[SUCCESS] run test "test.args" in 0.53 ms (status 0)
[KILLED] run test "test.assert" in 0.49 ms (signal "Aborted")
[FAILURE] run test "test.failure" in 0.40 ms (status 1)
[SUCCESS] run test "test.hello" in 0.47 ms (status 0)
[TIMEOUT] run test "test.infiniteloop" in 2000.19 ms (status 124)
[KILLED] run test "test.segfault" in 0.14 ms (signal "Segmentation fault")
[TIMEOUT] run test "test.sleep" in 2000.33 ms (status 124)
[SUCCESS] run test "test.success" in 0.42 ms (status 0)
=> 40% tests passed, 6 tests failed out of 10
```

If you prefer to run all tests in parallel (i.e. in concurrent processes), you can use the *forkp* mode. It will probably run faster, at the risk that the test outputs will be interleaved.

```bash
$ ./sample -O -t 2 -m forkp
[SUCCESS] run test "test.args" in 0.17 ms (status 0)
[FAILURE] run test "test.failure" in 0.27 ms (status 1)
[SUCCESS] run test "test.success" in 0.18 ms (status 0)
[KILLED] run test "test.assert" in 0.31 ms (signal "Aborted")
[SUCCESS] run test "test.hello" in 0.31 ms (status 0)
[KILLED] run test "test.segfault" in 0.46 ms (signal "Segmentation fault")
[KILLED] run test "test.alarm" in 1000.29 ms (signal "Alarm clock")
[TIMEOUT] run test "test.infiniteloop" in 2000.23 ms (status 124)
[TIMEOUT] run test "test.sleep" in 2000.15 ms (status 124)
=> 40% tests passed, 6 tests failed out of 10
```

### Run a single test

Let's run a *single test* instead of a *test suite* as follow:

```bash
$ ./sample -m forks -r test.failure -x
[FAILURE] run test "test.failure" in 0.43 ms (status 1)
=> 0% tests passed, 1 tests failed out of 1
```

As already explained, the *forks* mode starts each test separately in a forked process. The failure of a test will not affect the execution of the following tests.

Now, let's run a single test in *nofork* mode:

```bash
$ ./sample -m nofork  -r test.failure -x
[FAILURE] run test "test.failure" in 0.01 ms (status 1)
=> 0% tests passed, 1 tests failed out of 1
```

whereas

```bash
$ ./sample -m nofork  -r test.assert -x
Assertion failed: (1 == 2), function test_assert, file sample.c, line 41.
Abort trap: 6
```

## Using TestFW with CMake

In the *nofork* mode, each test is runned *directly* as a function call (without fork). As a consequence, the first test that fails will interrupt all the following.  It is especially useful when running all tests one by one within another test framework as CTest. See [CMakeLists.txt](CMakeLists.txt).

And running tests.

```bash
cmake . && make && ctest
```

## Writing your own Main Routine

A *main()* routine is already provided for convenience in the *libtestfw_main.a* library, but it could be useful in certain case to write your own *main()* routine based on the [testfw.h](testfw.h) API. See [sample_main.c](sample_main.c).

```c
#include <stdlib.h>
#include <stdbool.h>
#include "testfw.h"
#include "sample.h"

#define TIMEOUT 2
#define LOGFILE "test.log"
#define SILENT false

int main(int argc, char *argv[])
{
    struct testfw_t *fw = testfw_init(argv[0], TIMEOUT, LOGFILE, SILENT);
    testfw_register_func(fw, "test", "success", test_success);
    testfw_register_symb(fw, "test", "failure");
    testfw_register_suite(fw, "othertest");
    testfw_run_all(fw, argc - 1, argv + 1, TESTFW_FORK);
    testfw_free(fw);
    return EXIT_SUCCESS;
}
```

Compiling and running this test will produce the following results.

```bash
$ gcc -std=c99 -Wall sample.c sample_main.c -o sample_main -rdynamic -ltestfw -ldl -L.
$ ./sample_main
[SUCCESS] run test "test.success" in 0.24 ms (status 0)
[FAILURE] run test "test.failure" in 0.29 ms (status 1)
[FAILURE] run test "othertest.failure" in 0.09 ms (status 1)
[SUCCESS] run test "othertest.success" in 0.17 ms (status 0)
=> 50% tests passed, 2 tests failed out of 4
```
