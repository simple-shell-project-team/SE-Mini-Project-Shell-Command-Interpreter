CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

.PHONY: all test clean

all: libmysh_parser.a

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c -o parser.o

libmysh_parser.a: parser.o
	$(AR) rcs libmysh_parser.a parser.o

tests/test_parser: parser.c parser.h tests/test_parser.c
	$(CC) $(CFLAGS) parser.c tests/test_parser.c -o tests/test_parser

test: tests/test_parser
	./tests/test_parser

clean:
	rm -f parser.o libmysh_parser.a tests/test_parser
