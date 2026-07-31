#include "common.h"
#include "protocol.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int server_fd = -1;

// POSIX System Call helpers for complete socket reads and writes
static ssize_t read_bytes(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *ptr = (char *)buf;
    while (total < count) {
        ssize_t n = read(fd, ptr + total, count - total);
        if (n <= 0) return n; // 0 = EOF, -1 = Error
        total += n;
    }
    return total;
}

static ssize_t write_bytes(int fd, const void *buf, size_t count) {
    size_t total = 0;
    const char *ptr = (const char *)buf;
    while (total < count) {
        ssize_t n = write(fd, ptr + total, count - total);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

// Signal handler for graceful shutdown
static void handle_signal(int sig) {
    (void)sig;
    printf("\nShutting down banking server...\n");
    if (server_fd != -1) {
        close(server_fd);
    }
    exit(0);
}

// Per-client thread handler
static void *handle_client(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    RequestPacket req;
    ResponsePacket res;

    printf("[Server Thread %lu] Handling client connection on FD %d\n", (unsigned long)pthread_self(), client_fd);

    while (1) {
        memset(&req, 0, sizeof(req));
        ssize_t n = read_bytes(client_fd, &req, sizeof(RequestPacket));
        if (n <= 0) {
            printf("[Server Thread %lu] Client FD %d disconnected.\n", (unsigned long)pthread_self(), client_fd);
            break;
        }

        if (req.opcode == OP_EXIT) {
            printf("[Server Thread %lu] Client FD %d sent OP_EXIT.\n", (unsigned long)pthread_self(), client_fd);
            memset(&res, 0, sizeof(res));
            res.status_code = STATUS_SUCCESS;
            strncpy(res.message, "Goodbye!", sizeof(res.message) - 1);
            write_bytes(client_fd, &res, sizeof(ResponsePacket));
            break;
        }

        // Opcode Dispatcher skeleton (Milestone 2 basic ping / status handling)
        memset(&res, 0, sizeof(res));
        res.status_code = STATUS_SUCCESS;
        snprintf(res.message, sizeof(res.message), "Server received opcode %d successfully", req.opcode);

        write_bytes(client_fd, &res, sizeof(ResponsePacket));
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    // Register signal handler
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize database files
    if (db_init() != 0) {
        fprintf(stderr, "Failed to initialize storage engine.\n");
        return 1;
    }

    // 1. Create TCP Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;
    }

    // 2. Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }

    // 3. Bind to Address and Port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    // 4. Listen for Incoming Connections
    if (listen(server_fd, 10) == -1) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    printf("===========================================\n");
    printf("   Banking System Server Listening on Port %d\n", port);
    printf("===========================================\n");

    // 5. Accept Loop with pthread_create and pthread_detach
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            perror("accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Accepted connection from %s:%d (FD: %d)\n", client_ip, ntohs(client_addr.sin_port), client_fd);

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)(intptr_t)client_fd) != 0) {
            perror("pthread_create failed");
            close(client_fd);
        } else {
            pthread_detach(tid);
        }
    }

    close(server_fd);
    return 0;
}
