#include "common.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

int main(int argc, char *argv[]) {
    const char *server_ip = "127.0.0.1";
    int port = DEFAULT_PORT;

    if (argc > 1) {
        server_ip = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }

    // 1. Create Socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket creation failed");
        return 1;
    }

    // 2. Connect to Server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address: %s\n", server_ip);
        close(sock_fd);
        return 1;
    }

    printf("Connecting to banking server at %s:%d...\n", server_ip, port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to server failed");
        close(sock_fd);
        return 1;
    }
    printf("Connected to server successfully!\n\n");

    // Interactive CLI Loop
    int choice;
    while (1) {
        printf("===========================================\n");
        printf("       BANKING SYSTEM - CLIENT CLI\n");
        printf("===========================================\n");
        printf("1. Test Server Ping Connection\n");
        printf("2. Exit\n");
        printf("Enter choice (1-2): ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // clear buffer
            continue;
        }

        if (choice == 1) {
            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = 99; // Test ping opcode

            if (write_bytes(sock_fd, &req, sizeof(req)) <= 0) {
                printf("Failed to send request to server.\n");
                break;
            }

            memset(&res, 0, sizeof(res));
            if (read_bytes(sock_fd, &res, sizeof(res)) <= 0) {
                printf("Server closed connection.\n");
                break;
            }

            printf("\n[Server Response] Status: %d, Message: %s\n\n", res.status_code, res.message);
        } else if (choice == 2) {
            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = OP_EXIT;

            write_bytes(sock_fd, &req, sizeof(req));
            read_bytes(sock_fd, &res, sizeof(res));
            printf("Server: %s\nDisconnecting...\n", res.message);
            break;
        } else {
            printf("Invalid choice. Try again.\n\n");
        }
    }

    close(sock_fd);
    return 0;
}
