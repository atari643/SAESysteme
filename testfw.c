#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "testfw.h"

#define GREEN "\033[0;32m" // Green Color
#define RED "\033[0;31m"   // Red Color
#define NC "\033[0m"       // No Color

/* ********** STRUCTURES ********** */

struct testfw_t
{
    char *program;
    int timeout;
    char *logfile;
    bool silent;
    int size;
    int capacity;
    struct test_t *tests;
};

/* ********** FRAMEWORK ********** */

struct testfw_t *testfw_init(char *program, int timeout, char *logfile, bool silent)
{
    assert(program);
    struct testfw_t *fw = malloc(sizeof(struct testfw_t));
    assert(fw);
    fw->program = strdup(program);
    fw->timeout = timeout;
    fw->logfile = logfile ? strdup(logfile) : NULL;
    fw->silent = silent;
    fw->size = 0;
    fw->capacity = 10;
    fw->tests = malloc(fw->capacity * sizeof(struct test_t));
    assert(fw->tests);
    return fw;
}

void testfw_free(struct testfw_t *fw)
{
    assert(fw);
    free(fw->program);
    free(fw->logfile);
    for (int i = 0; i < fw->size; i++)
        free(fw->tests[i].name);
    free(fw->tests);
    free(fw);
}

int testfw_length(struct testfw_t *fw)
{
    assert(fw);
    return fw->size;
}

struct test_t *testfw_get(struct testfw_t *fw, int k)
{
    assert(fw);
    assert(k >= 0 && k < fw->size);
    return fw->tests + k;
}

/* ********** REGISTER TEST ********** */

static struct test_t *add_test(struct testfw_t *fw, char *suite, char *name, testfw_func_t func)
{
    assert(fw && fw->size <= fw->capacity);
    assert(suite && name && func);

    if (fw->size == fw->capacity)
    {
        fw->capacity *= 2;
        fw->tests = realloc(fw->tests, fw->capacity * sizeof(struct test_t));
        assert(fw->tests);
    }
    struct test_t *t = &(fw->tests[fw->size]);
    t->suite = strdup(suite);
    t->name = strdup(name);
    t->func = func;
    fw->size++;
    return t;
}

struct test_t *testfw_register_func(struct testfw_t *fw, char *suite, char *name, testfw_func_t func)
{
    assert(fw);
    assert(name);
    assert(func);
    struct test_t *t = add_test(fw, suite, name, func);
    return t;
}

static testfw_func_t lookup_symb(void *handle, char *funcname)
{
    assert(handle);
    testfw_func_t func = (testfw_func_t)dlsym(handle, funcname);
    if (!func)
    {
        fprintf(stderr, "Error: symbol \"%s\" not found!\n", funcname);
        exit(EXIT_FAILURE);
    }
    return func;
}

static char *test2func(char *suite, char *name)
{
    char *funcname = malloc((strlen(name) + 1 + strlen(suite) + 1) * sizeof(char));
    assert(funcname);
    *funcname = 0;
    strcat(funcname, suite);
    strcat(funcname, "_");
    strcat(funcname, name);
    return funcname;
}

struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name)
{
    assert(fw);
    assert(suite && name);
    void *handle = dlopen(NULL, RTLD_LAZY); /* if NULL, then the returned handle is for the main program */
    assert(handle);
    char *funcname = test2func(suite, name);
    testfw_func_t func = lookup_symb(handle, funcname);
    struct test_t *t = add_test(fw, suite, name, func);
    free(funcname);
    dlclose(handle);
    return t;
}

static char **discover_all_tests(struct testfw_t *fw, char *suite)
{
    assert(fw);
    void *handle = dlopen(NULL, RTLD_LAZY); /* if NULL, then the returned handle is for the main program */
    assert(handle);
    int nbtests = 0;
    int maxtests = 10;
    char **names = malloc((maxtests + 1) * sizeof(char *));
    assert(names);
    names[0] = NULL;

    char *cmdline = NULL;

    char *prefix_ = malloc(strlen(suite) + 2); /* adding a trailing '_' to suite */
    assert(prefix_);
#if defined(__APPLE__) && defined(__MACH__)
    strcpy(prefix_, "_");
    strcat(prefix_, suite);
    strcat(prefix_, "_");
#else
    strcpy(prefix_, suite);
    strcat(prefix_, "_");
#endif

    /* TODO: inspect symbol table instead of using nm external command */
    asprintf(&cmdline, "nm --defined-only %s | cut -d ' ' -f 3 | grep ^%s ", fw->program, prefix_);
    // printf("cmdline: %s\n", cmdline);
    assert(cmdline);
    FILE *stream = popen(cmdline, "r");
    char *funcname = NULL;
    size_t size = 0;
    while (getline(&funcname, &size, stream) > 0)
    {
        if (nbtests == maxtests)
        {
            maxtests *= 2;
            names = realloc(names, (maxtests + 1) * sizeof(char *));
            assert(names);
        }
        funcname[strlen(funcname) - 1] = 0; /* remove trailing \n */
        char *name = strstr(funcname, prefix_);
        assert(name);
        name += strlen(prefix_);
        // printf("discover test: %s\n", name);
        names[nbtests] = strdup(name);
        names[nbtests + 1] = NULL;
        nbtests++;
    }
    free(funcname);
    free(cmdline);
    pclose(stream);
    dlclose(handle);
    return names;
}

int testfw_register_suite(struct testfw_t *fw, char *suite)
{
    assert(fw);
    assert(suite);
    char **names = discover_all_tests(fw, suite);
    void *handle = dlopen(NULL, RTLD_LAZY); /* if NULL, then the returned handle is for the main program */
    assert(handle);
    char **t = names;
    int k = 0;
    while (*t)
    {
        char *name = *t;
        char *funcname = test2func(suite, name);
        testfw_func_t func = lookup_symb(handle, funcname);
        add_test(fw, suite, name, func);
        free(name);
        free(funcname);
        t++;
        k++;
    }
    dlclose(handle);
    free(names);
    return k;
}

/* ********** DIAGNOSTIC ********** */

static void print_diag_test(FILE *stream, struct test_t *t, int wstatus, double mtime)
{
    assert(stream);
    assert(t);

    if (WIFEXITED(wstatus))
    {
        int status = WEXITSTATUS(wstatus);
        if (status == TESTFW_EXIT_SUCCESS)
            fprintf(stream, "%s[SUCCESS]%s run test \"%s.%s\" in %.2f ms (status %d)\n", GREEN, NC, t->suite, t->name, mtime, status);
        else if (status == TESTFW_EXIT_TIMEOUT)
            fprintf(stream, "%s[TIMEOUT]%s run test \"%s.%s\" in %.2f ms (status %d)\n", RED, NC, t->suite, t->name, mtime, status);
        else
            fprintf(stream, "%s[FAILURE]%s run test \"%s.%s\" in %.2f ms (status %d)\n", RED, NC, t->suite, t->name, mtime, status);
    }
    else if (WIFSIGNALED(wstatus))
    {
        int sig = WTERMSIG(wstatus);
        fprintf(stream, "%s[KILLED]%s run test \"%s.%s\" in %.2f ms (signal \"%s\")\n", RED, NC, t->suite, t->name, mtime, strsignal(sig));
    }
    else
        assert(0); // you should not be here?
}
static void write_in_log(struct testfw_t *fw, struct test_t *t, int wstatus, double mtime){
     if (access(fw->logfile, F_OK) != -1) {
            printf("Le fichier existe\n");
            if (access(fw->logfile, W_OK) != -1) {
                // Vous pouvez écrire dans le fichier
                FILE *fichier = fopen(fw->logfile, "a");
                print_diag_test(fichier, t, wstatus, mtime);

            } else {
                printf("Vous n'avez pas les droits d'écriture\n");
            }
        } else {
            printf("Le fichier n'existe pas\n");
        }
}

/* ********** RUN TEST (NOFORK MODE) ********** */

static int run_test_nofork(struct testfw_t *fw, struct test_t *t, int argc, char *argv[])
{
    assert(fw);
    assert(t);
    int wstatus = 0;

    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);

    wstatus = t->func(argc, argv);

    gettimeofday(&tv_end, NULL);
    double mtime = (tv_end.tv_sec - tv_start.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_start.tv_usec) / 1000.0; // in ms

    if (!fw->silent)
        print_diag_test(stdout, t, wstatus, mtime);
    else{
        if(fw->logfile!=NULL){
            write_in_log(fw, t, wstatus, mtime);
        }
    }

    return EXIT_SUCCESS;
    (void)argc;
    (void)argv;
}

/* ********** RUN TEST (FORK MODE) ********** */

static int run_test_forks(struct testfw_t *fw, struct test_t *t, int argc, char *argv[])
{
    assert(fw);
    assert(t);
    int wstatus = 0;
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
    
    pid_t pid = fork();
    if (pid == 0)
    {
        pid_t pid_test = fork();
        if (pid_test == 0)
        {
            exit(t->func(argc, argv));
        }
        if(fw->timeout==0){
            waitpid(pid_test, &wstatus, 0);
        }else{
        pid_t pid_timeout = fork();
        if (pid_timeout == 0)
        {
            sleep(fw->timeout);
            kill(pid_test, SIGUSR1);
            exit(TESTFW_EXIT_TIMEOUT);
        }
        waitpid(pid_test, &wstatus, 0);
        if (WTERMSIG(wstatus) == SIGUSR1)//SIGUSR1 est le signal que renvoie Crlr+C qu'on effectue généralement pour arrêter un processus trop long
        {
            exit(TESTFW_EXIT_TIMEOUT);
        
        }
        else if (WIFSIGNALED(wstatus))
        {
           kill(getpid(), WTERMSIG(wstatus));//On simule le kill qui a fait que le processus fils ne s'est pas terminer
        }
        else
        {
            exit(WEXITSTATUS(wstatus));
        }
        }
    }
    waitpid(pid, &wstatus, 0);
    gettimeofday(&tv_end, NULL);
    double mtime = (tv_end.tv_sec - tv_start.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_start.tv_usec) / 1000.0; // in ms

    if (!fw->silent){
        print_diag_test(stdout, t, wstatus, mtime);
    }else{
        if(fw->logfile!=NULL){
            write_in_log(fw, t, wstatus, mtime);
        }
    }
    

    if(wstatus!=0){
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
    (void)argc;
    (void)argv;
}
/* ********** RUN TEST (FORKP MODE) ********** */

static int run_test_forkp(struct testfw_t *fw, struct test_t *t, int argc, char *argv[])
{
    assert(fw != NULL);
    assert(t != NULL);

    
    return EXIT_SUCCESS;
}


/* ********** RUN TEST ********** */

static int run_test(struct testfw_t *fw, struct test_t *t, int argc, char *argv[], enum testfw_mode_t mode)
{
    if (!fw->silent)
        printf("******************** RUN TEST \"%s.%s\" ********************\n", t->suite, t->name);

    switch (mode)
    {
    case TESTFW_NOFORK:
        return run_test_nofork(fw, t, argc, argv);
    case TESTFW_FORKS:
        return run_test_forks(fw, t, argc, argv);
    case TESTFW_FORKP:
        return run_test_forkp(fw, t, argc, argv);
    default:
        fprintf(stderr, "Error: invalid execution mode (%d)!\n", mode);
        exit(EXIT_FAILURE);
    }

    return EXIT_FAILURE;
}

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode)
{
    assert(fw);
    int nfailures = 0;
    for (int i = 0; i < fw->size; i++)
    {
        struct test_t *t = &fw->tests[i];
        assert(t);
        nfailures += run_test(fw, t, argc, argv, mode);
        
    }

    return nfailures;
}
