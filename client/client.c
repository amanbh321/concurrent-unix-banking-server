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

static const char* role_to_string(UserRole role) {
    switch (role) {
        case ROLE_CUSTOMER: return "Customer";
        case ROLE_EMPLOYEE: return "Bank Employee";
        case ROLE_MANAGER:  return "Manager";
        case ROLE_ADMIN:    return "Administrator";
        default:            return "Unknown";
    }
}

static void show_customer_menu(int sock_fd, const UserRecord *user) {
    int choice;
    while (1) {
        printf("===========================================\n");
        printf("       CUSTOMER MENU - %s\n", user->full_name);
        printf("===========================================\n");
        printf("1. View Account Balance\n");
        printf("2. Deposit Money\n");
        printf("3. Withdraw Money\n");
        printf("4. Logout\n");
        printf("5. Exit System\n");
        printf("Enter choice (1-5): ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (choice == 1) {
            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = OP_VIEW_BALANCE;
            req.user_id = user->user_id;
            req.role = user->role;

            if (write_bytes(sock_fd, &req, sizeof(req)) <= 0) break;
            memset(&res, 0, sizeof(res));
            if (read_bytes(sock_fd, &res, sizeof(res)) <= 0) break;

            printf("\n[Server Response] %s\n", res.message);
            if (res.status_code == STATUS_SUCCESS) {
                printf("Account ID: %d | Status: ACTIVE | Current Balance: $%.2f\n", 
                       res.payload.account.account_id, res.payload.account.balance);
            }
            printf("\n");
        } else if (choice == 2) {
            double amount;
            printf("\nEnter Deposit Amount: $");
            if (scanf("%lf", &amount) != 1) {
                while (getchar() != '\n');
                continue;
            }

            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = OP_DEPOSIT;
            req.user_id = user->user_id;
            req.role = user->role;
            req.payload.deposit.amount = amount;

            if (write_bytes(sock_fd, &req, sizeof(req)) <= 0) break;
            memset(&res, 0, sizeof(res));
            if (read_bytes(sock_fd, &res, sizeof(res)) <= 0) break;

            printf("\n[Server Response] %s\n\n", res.message);
        } else if (choice == 3) {
            double amount;
            printf("\nEnter Withdrawal Amount: $");
            if (scanf("%lf", &amount) != 1) {
                while (getchar() != '\n');
                continue;
            }

            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = OP_WITHDRAW;
            req.user_id = user->user_id;
            req.role = user->role;
            req.payload.withdraw.amount = amount;

            if (write_bytes(sock_fd, &req, sizeof(req)) <= 0) break;
            memset(&res, 0, sizeof(res));
            if (read_bytes(sock_fd, &res, sizeof(res)) <= 0) break;

            printf("\n[Server Response] %s\n\n", res.message);
        } else if (choice == 4) {
            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = OP_LOGOUT;
            req.user_id = user->user_id;
            req.role = user->role;

            write_bytes(sock_fd, &req, sizeof(req));
            read_bytes(sock_fd, &res, sizeof(res));
            printf("\n[Server Response] %s\n\n", res.message);
            break;
        } else if (choice == 5) {
            RequestPacket req;
            ResponsePacket res;
            memset(&req, 0, sizeof(req));
            req.opcode = OP_EXIT;
            req.user_id = user->user_id;
            req.role = user->role;

            write_bytes(sock_fd, &req, sizeof(req));
            read_bytes(sock_fd, &res, sizeof(res));
            printf("Server: %s\nDisconnecting...\n", res.message);
            exit(0);
        } else {
            printf("Invalid choice. Try again.\n\n");
        }
    }
}

int main(int argc, char *argv[]) {
    const char *server_ip = "127.0.0.1";
    int port = DEFAULT_PORT;

    if (argc > 1) server_ip = argv[1];
    if (argc > 2) port = atoi(argv[2]);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket creation failed");
        return 1;
    }

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

    int is_logged_in = 0;
    UserRecord current_user;
    memset(&current_user, 0, sizeof(current_user));

    int choice;
    while (1) {
        printf("===========================================\n");
        printf("       BANKING MANAGEMENT SYSTEM CLI\n");
        printf("===========================================\n");

        if (!is_logged_in) {
            printf("1. Login\n");
            printf("2. Exit\n");
            printf("Enter choice (1-2): ");
            if (scanf("%d", &choice) != 1) {
                while (getchar() != '\n');
                continue;
            }

            if (choice == 1) {
                RequestPacket req;
                ResponsePacket res;
                memset(&req, 0, sizeof(req));
                req.opcode = OP_LOGIN;

                printf("\n--- LOGIN ---\n");
                printf("Enter Username: ");
                scanf("%31s", req.payload.login.username);
                printf("Enter Password: ");
                scanf("%63s", req.payload.login.password);

                if (write_bytes(sock_fd, &req, sizeof(req)) <= 0) {
                    printf("Server connection lost.\n");
                    break;
                }

                memset(&res, 0, sizeof(res));
                if (read_bytes(sock_fd, &res, sizeof(res)) <= 0) {
                    printf("Server closed connection.\n");
                    break;
                }

                printf("\n[Server Response] %s\n", res.message);
                if (res.status_code == STATUS_SUCCESS) {
                    is_logged_in = 1;
                    current_user = res.payload.user;
                    printf("Session active for User ID: %d | Role: %s\n\n", 
                           current_user.user_id, role_to_string(current_user.role));

                    if (current_user.role == ROLE_CUSTOMER) {
                        show_customer_menu(sock_fd, &current_user);
                        is_logged_in = 0;
                        memset(&current_user, 0, sizeof(current_user));
                    }
                } else {
                    printf("\n");
                }
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
        } else {
            // General logged-in view for other roles (Employee, Manager, Admin)
            printf("Logged in as: %s (ID: %d, Role: %s)\n", 
                   current_user.full_name, current_user.user_id, role_to_string(current_user.role));
            printf("-------------------------------------------\n");
            printf("1. Logout\n");
            printf("2. Exit\n");
            printf("Enter choice (1-2): ");
            if (scanf("%d", &choice) != 1) {
                while (getchar() != '\n');
                continue;
            }

            if (choice == 1) {
                RequestPacket req;
                ResponsePacket res;
                memset(&req, 0, sizeof(req));
                req.opcode = OP_LOGOUT;
                req.user_id = current_user.user_id;
                req.role = current_user.role;

                if (write_bytes(sock_fd, &req, sizeof(req)) <= 0) break;

                memset(&res, 0, sizeof(res));
                read_bytes(sock_fd, &res, sizeof(res));
                printf("\n[Server Response] %s\n\n", res.message);

                is_logged_in = 0;
                memset(&current_user, 0, sizeof(current_user));
            } else if (choice == 2) {
                RequestPacket req;
                ResponsePacket res;
                memset(&req, 0, sizeof(req));
                req.opcode = OP_EXIT;
                req.user_id = current_user.user_id;
                req.role = current_user.role;

                write_bytes(sock_fd, &req, sizeof(req));
                read_bytes(sock_fd, &res, sizeof(res));
                printf("Server: %s\nDisconnecting...\n", res.message);
                break;
            } else {
                printf("Invalid choice. Try again.\n\n");
            }
        }
    }

    close(sock_fd);
    return 0;
}
