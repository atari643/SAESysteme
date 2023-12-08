COMMON_FLAGS := -g -std=c99 -Wall -Wextra -pedantic

SRC      := testfw.c
OBJ      := ${SRC:.c=.o}
LIBNAME  := testfw
MAIN     := testfw_main
MAIN_OBJ := $(MAIN).o

CC       := gcc
CFLAGS   := $(COMMON_FLAGS)
LDFLAGS  := $(COMMON_FLAGS) -rdynamic -ldl -L.

NAME     := $(LIBNAME)_$(shell uname -m)-$(shell uname -s)
SNAME    := lib$(NAME).a
LDLIBS   := -l$(NAME)
AR       := ar
ARFLAGS  := rcs

all: $(SNAME) hello sample sample_main

$(SNAME): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

hello: hello.c $(MAIN_OBJ) $(SNAME)
	$(CC) $(CFLAGS) hello.c $(MAIN_OBJ) $(LDFLAGS) $(LDLIBS) -o $@

sample: sample.c $(MAIN_OBJ) $(SNAME)
	$(CC) $(CFLAGS) sample.c $(MAIN_OBJ) $(LDFLAGS) $(LDLIBS) -o $@

sample_main: sample.c sample_main.c $(SNAME)
	$(CC) $(CFLAGS) sample.c sample_main.c $(LDFLAGS) $(LDLIBS) -o $@

-include ${OBJ:.o=.d}

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c -o $*.o

clean:
	@rm -f $(OBJ) $(MAIN_OBJ) $(SNAME) hello sample sample_main *.log
	@rm -rf build *.dSYM
