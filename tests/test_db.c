#include "db.h"
#include "common.h"
#include "models.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

int main(void) {
    printf("=== Starting Milestone 1 Storage Engine Tests ===\n");

    // 1. Initialize Database
    assert(db_init() == 0);
    printf("[PASS] db_init() completed successfully\n");

    // 2. Verify Bootstrap Admin (ID 1000)
    UserRecord admin;
    assert(db_find_user_by_id(1000, &admin) == 0);
    assert(strcmp(admin.username, "admin") == 0);
    assert(admin.role == ROLE_ADMIN);
    printf("[PASS] Admin bootstrap verified (User ID: %d, Username: %s)\n", admin.user_id, admin.username);

    // 3. Verify Atomic ID Generation
    int u_id1 = db_get_next_user_id();
    int u_id2 = db_get_next_user_id();
    assert(u_id1 == 1001);
    assert(u_id2 == 1002);

    int acc_id1 = db_get_next_account_id();
    assert(acc_id1 == 5001);
    printf("[PASS] Atomic ID counters verified (User IDs: %d, %d; Account ID: %d)\n", u_id1, u_id2, acc_id1);

    // 4. Test Customer Record Creation & Find
    UserRecord cust1;
    memset(&cust1, 0, sizeof(cust1));
    cust1.user_id = u_id1;
    strncpy(cust1.username, "john_doe", sizeof(cust1.username) - 1);
    strncpy(cust1.password, "pass123", sizeof(cust1.password) - 1);
    cust1.role = ROLE_CUSTOMER;
    cust1.account_id = acc_id1;
    cust1.age = 28;
    cust1.is_active = 1;
    strncpy(cust1.full_name, "John Doe", sizeof(cust1.full_name) - 1);
    strncpy(cust1.gender, "Male", sizeof(cust1.gender) - 1);
    strncpy(cust1.phone, "9876543210", sizeof(cust1.phone) - 1);
    strncpy(cust1.email, "john@example.com", sizeof(cust1.email) - 1);
    strncpy(cust1.address, "123 Main St", sizeof(cust1.address) - 1);

    assert(db_write_user(&cust1) == 0);

    UserRecord found_user;
    assert(db_find_user_by_username("john_doe", &found_user) == 0);
    assert(found_user.user_id == u_id1);
    assert(found_user.account_id == acc_id1);
    printf("[PASS] Customer record creation & username lookup verified (User ID: %d)\n", found_user.user_id);

    // 5. Test Account Creation & Update
    AccountRecord acc1;
    memset(&acc1, 0, sizeof(acc1));
    acc1.account_id = acc_id1;
    acc1.customer_id = u_id1;
    acc1.status = ACCOUNT_ACTIVE;
    acc1.balance = 1500.50;

    assert(db_write_account(&acc1) == 0);

    AccountRecord found_acc;
    assert(db_find_account_id(acc_id1, &found_acc) == 0);
    assert(found_acc.balance == 1500.50);

    // Update balance
    found_acc.balance += 500.00; // 2000.50
    assert(db_update_account(&found_acc) == 0);

    AccountRecord updated_acc;
    assert(db_find_account_id(acc_id1, &updated_acc) == 0);
    assert(updated_acc.balance == 2000.50);
    printf("[PASS] Account creation & balance update verified (New Balance: %.2f)\n", updated_acc.balance);

    // 6. Test Transaction Logging
    TransactionRecord txn;
    memset(&txn, 0, sizeof(txn));
    txn.transaction_id = db_get_next_transaction_id();
    txn.source_account_id = 0;
    txn.destination_account_id = acc_id1;
    txn.type = TXN_DEPOSIT;
    txn.amount = 500.00;
    txn.balance_after = 2000.50;
    txn.timestamp = (long)time(NULL);

    assert(db_write_transaction(&txn) == 0);

    TransactionRecord txns[10];
    int count = 0;
    assert(db_get_transactions_by_account_id(acc_id1, txns, 10, &count) == 0);
    assert(count == 1);
    assert(txns[0].amount == 500.00);
    printf("[PASS] Transaction record logging & querying verified (Txn ID: %d, Count: %d)\n", txns[0].transaction_id, count);

    // 7. Test Loan Application Workflow Primitives
    LoanRecord loan;
    memset(&loan, 0, sizeof(loan));
    loan.loan_id = db_get_next_loan_id();
    loan.customer_id = u_id1;
    loan.account_id = acc_id1;
    loan.status = LOAN_PENDING;
    loan.assigned_employee_id = 0;
    loan.amount = 10000.00;

    assert(db_write_loan(&loan) == 0);

    LoanRecord pending_loans[10];
    int loan_count = 0;
    assert(db_get_all_pending_loans(pending_loans, 10, &loan_count) == 0);
    assert(loan_count == 1);
    assert(pending_loans[0].amount == 10000.00);
    printf("[PASS] Loan application logging & pending scan verified (Loan ID: %d)\n", pending_loans[0].loan_id);

    printf("=== ALL MILESTONE 1 STORAGE ENGINE TESTS PASSED ===\n");
    return 0;
}
