#include "customer_ops.h"
#include "db.h"
#include "models.h"
#include "common.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void handle_customer_view_balance(int client_fd, const RequestPacket *req, ResponsePacket *res) {
    (void)client_fd;
    UserRecord user;
    if (db_find_user_by_id(req->user_id, &user) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "User profile not found.", sizeof(res->message) - 1);
        return;
    }

    AccountRecord account;
    if (db_find_account_by_customer_id(user.user_id, &account) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "No active bank account associated with this user.", sizeof(res->message) - 1);
        return;
    }

    if (account.status != ACCOUNT_ACTIVE) {
        res->status_code = STATUS_ACCOUNT_INACTIVE;
        strncpy(res->message, "Account is deactivated. Operations restricted.", sizeof(res->message) - 1);
        return;
    }

    res->status_code = STATUS_SUCCESS;
    snprintf(res->message, sizeof(res->message), "Account Balance: $%.2f", account.balance);
    res->record_count = 1;
    res->payload.account = account;
}

void handle_customer_deposit(int client_fd, const RequestPacket *req, ResponsePacket *res) {
    (void)client_fd;
    double amount = req->payload.deposit.amount;
    if (amount <= 0.0) {
        res->status_code = STATUS_INVALID_INPUT;
        strncpy(res->message, "Deposit amount must be positive.", sizeof(res->message) - 1);
        return;
    }

    UserRecord user;
    if (db_find_user_by_id(req->user_id, &user) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "User profile not found.", sizeof(res->message) - 1);
        return;
    }

    AccountRecord account;
    if (db_find_account_by_customer_id(user.user_id, &account) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "No active bank account found for deposit.", sizeof(res->message) - 1);
        return;
    }

    if (account.status != ACCOUNT_ACTIVE) {
        res->status_code = STATUS_ACCOUNT_INACTIVE;
        strncpy(res->message, "Account is deactivated.", sizeof(res->message) - 1);
        return;
    }

    account.balance += amount;

    if (db_update_account(&account) != 0) {
        res->status_code = STATUS_FAILURE;
        strncpy(res->message, "Failed to update account balance in storage.", sizeof(res->message) - 1);
        return;
    }

    // Log transaction
    TransactionRecord txn;
    memset(&txn, 0, sizeof(txn));
    txn.transaction_id = db_get_next_transaction_id();
    txn.source_account_id = 0;
    txn.destination_account_id = account.account_id;
    txn.type = TXN_DEPOSIT;
    txn.amount = amount;
    txn.balance_after = account.balance;
    txn.timestamp = (long)time(NULL);
    db_write_transaction(&txn);

    res->status_code = STATUS_SUCCESS;
    snprintf(res->message, sizeof(res->message), "Successfully deposited $%.2f. New Balance: $%.2f", amount, account.balance);
    res->record_count = 1;
    res->payload.account = account;
}

void handle_customer_withdraw(int client_fd, const RequestPacket *req, ResponsePacket *res) {
    (void)client_fd;
    double amount = req->payload.withdraw.amount;
    if (amount <= 0.0) {
        res->status_code = STATUS_INVALID_INPUT;
        strncpy(res->message, "Withdrawal amount must be positive.", sizeof(res->message) - 1);
        return;
    }

    UserRecord user;
    if (db_find_user_by_id(req->user_id, &user) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "User profile not found.", sizeof(res->message) - 1);
        return;
    }

    AccountRecord account;
    if (db_find_account_by_customer_id(user.user_id, &account) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "No active bank account found for withdrawal.", sizeof(res->message) - 1);
        return;
    }

    if (account.status != ACCOUNT_ACTIVE) {
        res->status_code = STATUS_ACCOUNT_INACTIVE;
        strncpy(res->message, "Account is deactivated.", sizeof(res->message) - 1);
        return;
    }

    if (account.balance < amount) {
        res->status_code = STATUS_INSUFFICIENT_FUNDS;
        snprintf(res->message, sizeof(res->message), "Insufficient funds. Current balance: $%.2f", account.balance);
        return;
    }

    account.balance -= amount;

    if (db_update_account(&account) != 0) {
        res->status_code = STATUS_FAILURE;
        strncpy(res->message, "Failed to update account balance in storage.", sizeof(res->message) - 1);
        return;
    }

    // Log transaction
    TransactionRecord txn;
    memset(&txn, 0, sizeof(txn));
    txn.transaction_id = db_get_next_transaction_id();
    txn.source_account_id = account.account_id;
    txn.destination_account_id = 0;
    txn.type = TXN_WITHDRAWAL;
    txn.amount = amount;
    txn.balance_after = account.balance;
    txn.timestamp = (long)time(NULL);
    db_write_transaction(&txn);

    res->status_code = STATUS_SUCCESS;
    snprintf(res->message, sizeof(res->message), "Successfully withdrew $%.2f. New Balance: $%.2f", amount, account.balance);
    res->record_count = 1;
    res->payload.account = account;
}
