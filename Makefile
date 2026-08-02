CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude

SERVER_OBJS = src/server.o src/db.o src/session.o src/customer_ops.o
DB_OBJS = src/db.o

all: server client_app tests/test_db tests/test_session tests/test_customer

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

client/%.o: client/%.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -c $< -o $@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

client_app: client/client.o
	$(CC) $(CFLAGS) $^ -o $@

tests/test_db: tests/test_db.o $(DB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

tests/test_session: tests/test_session.o src/session.o
	$(CC) $(CFLAGS) $^ -o $@

tests/test_customer: tests/test_customer.o src/customer_ops.o $(DB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test: tests/test_db tests/test_session tests/test_customer
	./tests/test_db
	./tests/test_session
	./tests/test_customer

clean:
	rm -f src/*.o client/*.o tests/*.o tests/test_db tests/test_session tests/test_customer data/*.dat server client_app

.PHONY: all test clean client_app server
