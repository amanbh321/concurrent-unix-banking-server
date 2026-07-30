# Concurrent Unix Banking Server

A multi-client, concurrent banking management server built in C for Linux/Unix environments.

## Features
- POSIX system calls & TCP socket programming
- Multi-client concurrency using pthreads
- Record-level file locking (`fcntl`) and binary `.dat` persistence
- Role-based access for Customers, Employees, Managers, and Administrators

## Build & Run
```bash
make
./server
./client
```
