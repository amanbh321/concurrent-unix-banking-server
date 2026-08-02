#ifndef CUSTOMER_OPS_H
#define CUSTOMER_OPS_H

#include "protocol.h"

void handle_customer_view_balance(int client_fd, const RequestPacket *req, ResponsePacket *res);
void handle_customer_deposit(int client_fd, const RequestPacket *req, ResponsePacket *res);
void handle_customer_withdraw(int client_fd, const RequestPacket *req, ResponsePacket *res);

#endif // CUSTOMER_OPS_H
