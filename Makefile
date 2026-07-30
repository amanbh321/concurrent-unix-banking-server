CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude

SRCS = src/db.c
OBJS = $(SRCS:.c=.o)

all: tests/test_db

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/test_db: tests/test_db.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test: tests/test_db
	./tests/test_db

clean:
	rm -f src/*.o tests/*.o tests/test_db data/*.dat server client

.PHONY: all test clean
