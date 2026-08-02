#include "common.h"
#include "protocol.h"
#include "db.h"
#include "session.h"
#include "customer_ops.h"

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

static ssize_t read_bytes(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *ptr = (char *)buf;
    while (total < count) {
        ssize_t n = read(fd, ptr + total, count - total);
        if (n <= 0) return n;
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

static void handle_signal(int sig) {
    (void)sig;
    printf("\nShutting down banking server...\n");
    if (server_fd != -1) {
        close(server_fd);
    }
    exit(0);
}

// Authentication Handlers
static void handle_login(int client_fd, const RequestPacket *req, ResponsePacket *res) {
    UserRecord user;
    if (db_find_user_by_username(req->payload.login.username, &user) != 0) {
        res->status_code = STATUS_AUTH_FAILED;
        strncpy(res->message, "Invalid username or password.", sizeof(res->message) - 1);
        return;
    }

    if (strcmp(user.password, req->payload.login.password) != 0) {
        res->status_code = STATUS_AUTH_FAILED;
        strncpy(res->message, "Invalid username or password.", sizeof(res->message) - 1);
        return;
    }

    if (user.is_active != 1) {
        res->status_code = STATUS_ACCOUNT_INACTIVE;
        strncpy(res->message, "Account is deactivated. Contact manager.", sizeof(res->message) - 1);
        return;
    }

    int session_res = session_add(user.user_id, client_fd, user.role);
    if (session_res == STATUS_ALREADY_LOGGED_IN) {
        res->status_code = STATUS_ALREADY_LOGGED_IN;
        strncpy(res->message, "User is already logged in from another active session.", sizeof(res->message) - 1);
        return;
    } else if (session_res != 0) {
        res->status_code = STATUS_FAILURE;
        strncpy(res->message, "Server session limit reached.", sizeof(res->message) - 1);
        return;
    }

    res->status_code = STATUS_SUCCESS;
    snprintf(res->message, sizeof(res->message), "Login successful. Welcome, %s!", user.full_name);
    res->record_count = 1;
    res->payload.user = user;
}

static void handle_logout(int client_fd, ResponsePacket *res) {
    session_remove_by_fd(client_fd);
    res->status_code = STATUS_SUCCESS;
    strncpy(res->message, "Logged out successfully.", sizeof(res->message) - 1);
}

static void *handle_client(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    RequestPacket req;
    ResponsePacket res;

    printf("[Server Thread %lu] Connection opened on FD %d\n", (unsigned long)pthread_self(), client_fd);

    while (1) {
        memset(&req, 0, sizeof(req));
        ssize_t n = read_bytes(client_fd, &req, sizeof(RequestPacket));
        if (n <= 0) {
            printf("[Server Thread %lu] Connection closed on FD %d\n", (unsigned long)pthread_self(), client_fd);
            break;
        }

        memset(&res, 0, sizeof(res));

        switch (req.opcode) {
            case OP_LOGIN:
                handle_login(client_fd, &req, &res);
                break;
            case OP_LOGOUT:
                handle_logout(client_fd, &res);
                break;
            case OP_VIEW_BALANCE:
                handle_customer_view_balance(client_fd, &req, &res);
                break;
            case OP_DEPOSIT:
                handle_customer_deposit(client_fd, &req, &res);
                break;
            case OP_WITHDRAW:
                handle_customer_withdraw(client_fd, &req, &res);
                break;
            case OP_EXIT:
                session_remove_by_fd(client_fd);
                res.status_code = STATUS_SUCCESS;
                strncpy(res.message, "Goodbye!", sizeof(res.message) - 1);
                write_bytes(client_fd, &res, sizeof(ResponsePacket));
                goto cleanup;
            default:
                res.status_code = STATUS_SUCCESS;
                snprintf(res.message, sizeof(res.message), "Server received opcode %d", req.opcode);
                break;
        }

        write_bytes(client_fd, &res, sizeof(ResponsePacket));
    }

cleanup:
    session_remove_by_fd(client_fd);
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (db_init() != 0) {
        fprintf(stderr, "Failed to initialize storage engine.\n");
        return 1;
    }

    session_init();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }

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

    if (listen(server_fd, 10) == -1) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    printf("===========================================\n");
    printf("   Banking System Server Listening on Port %d\n", port);
    printf("===========================================\n");

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
