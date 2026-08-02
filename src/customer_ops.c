#include "customer_ops.h"
#include "db.h"
#include "models.h"
#include "common.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// Helper: lookup customer's account and validate it's active
static int get_active_account(int user_id, AccountRecord *account, ResponsePacket *res) {
    if (db_find_account_by_customer_id(user_id, account) != 0) {
        res->status_code = STATUS_NOT_FOUND;
        strncpy(res->message, "No bank account found for this user.", sizeof(res->message) - 1);
        return -1;
    }
    if (account->status != ACCOUNT_ACTIVE) {
        res->status_code = STATUS_ACCOUNT_INACTIVE;
        strncpy(res->message, "Account is deactivated. Operations restricted.", sizeof(res->message) - 1);
        return -1;
    }
    return 0;
}

// Helper: log a transaction record
static void log_transaction(int src_acc, int dst_acc, TransactionType type, double amount, double balance_after) {
    TransactionRecord txn;
    memset(&txn, 0, sizeof(txn));
    txn.transaction_id = db_get_next_transaction_id();
    txn.source_account_id = src_acc;
    txn.destination_account_id = dst_acc;
    txn.type = type;
    txn.amount = amount;
    txn.balance_after = balance_after;
    txn.timestamp = (long)time(NULL);
    db_write_transaction(&txn);
}

void handle_customer_view_balance(int client_fd, const RequestPacket *req, ResponsePacket *res) {
    (void)client_fd;
    AccountRecord account;
    if (get_active_account(req->user_id, &account, res) != 0) return;

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

    AccountRecord account;
    if (get_active_account(req->user_id, &account, res) != 0) return;

    account.balance += amount;
    if (db_update_account(&account) != 0) {
        res->status_code = STATUS_FAILURE;
        strncpy(res->message, "Failed to update account.", sizeof(res->message) - 1);
        return;
    }

    log_transaction(0, account.account_id, TXN_DEPOSIT, amount, account.balance);

    res->status_code = STATUS_SUCCESS;
    snprintf(res->message, sizeof(res->message), "Deposited $%.2f. New Balance: $%.2f", amount, account.balance);
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

    AccountRecord account;
    if (get_active_account(req->user_id, &account, res) != 0) return;

    if (account.balance < amount) {
        res->status_code = STATUS_INSUFFICIENT_FUNDS;
        snprintf(res->message, sizeof(res->message), "Insufficient funds. Balance: $%.2f", account.balance);
        return;
    }

    account.balance -= amount;
    if (db_update_account(&account) != 0) {
        res->status_code = STATUS_FAILURE;
        strncpy(res->message, "Failed to update account.", sizeof(res->message) - 1);
        return;
    }

    log_transaction(account.account_id, 0, TXN_WITHDRAWAL, amount, account.balance);

    res->status_code = STATUS_SUCCESS;
    snprintf(res->message, sizeof(res->message), "Withdrew $%.2f. New Balance: $%.2f", amount, account.balance);
    res->record_count = 1;
    res->payload.account = account;
}
