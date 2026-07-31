CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude

DB_OBJS = src/db.o

all: server client_app tests/test_db

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

client/%.o: client/%.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -c $< -o $@

server: src/server.o $(DB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

client_app: client/client.o
	$(CC) $(CFLAGS) $^ -o $@

tests/test_db: tests/test_db.o $(DB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test: tests/test_db
	./tests/test_db

clean:
	rm -f src/*.o client/*.o tests/*.o tests/test_db data/*.dat server client_app

.PHONY: all test clean client_app server
