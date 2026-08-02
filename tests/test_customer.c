#include "customer_ops.h"
#include "db.h"
#include "models.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("=== Starting Customer Banking Operations Unit Tests ===\n");

    assert(db_init() == 0);

    // 1. Setup Customer and Account
    UserRecord cust;
    memset(&cust, 0, sizeof(cust));
    cust.user_id = db_get_next_user_id(); // 1001
    strncpy(cust.username, "cust_test", sizeof(cust.username) - 1);
    strncpy(cust.password, "pass123", sizeof(cust.password) - 1);
    cust.role = ROLE_CUSTOMER;
    cust.account_id = db_get_next_account_id(); // 5001
    cust.is_active = 1;
    strncpy(cust.full_name, "Alice Smith", sizeof(cust.full_name) - 1);

    assert(db_write_user(&cust) == 0);

    AccountRecord acc;
    memset(&acc, 0, sizeof(acc));
    acc.account_id = cust.account_id;
    acc.customer_id = cust.user_id;
    acc.status = ACCOUNT_ACTIVE;
    acc.balance = 1000.00;

    assert(db_write_account(&acc) == 0);

    // 2. Test View Balance
    RequestPacket req;
    ResponsePacket res;
    memset(&req, 0, sizeof(req));
    req.opcode = OP_VIEW_BALANCE;
    req.user_id = cust.user_id;
    req.role = cust.role;

    memset(&res, 0, sizeof(res));
    handle_customer_view_balance(0, &req, &res);
    assert(res.status_code == STATUS_SUCCESS);
    assert(res.payload.account.balance == 1000.00);
    printf("[PASS] View balance verified (Balance: $%.2f)\n", res.payload.account.balance);

    // 3. Test Deposit $500
    memset(&req, 0, sizeof(req));
    req.opcode = OP_DEPOSIT;
    req.user_id = cust.user_id;
    req.role = cust.role;
    req.payload.deposit.amount = 500.00;

    memset(&res, 0, sizeof(res));
    handle_customer_deposit(0, &req, &res);
    assert(res.status_code == STATUS_SUCCESS);
    assert(res.payload.account.balance == 1500.00);
    printf("[PASS] Deposit verified (New Balance: $%.2f)\n", res.payload.account.balance);

    // 4. Test Withdraw $200
    memset(&req, 0, sizeof(req));
    req.opcode = OP_WITHDRAW;
    req.user_id = cust.user_id;
    req.role = cust.role;
    req.payload.withdraw.amount = 200.00;

    memset(&res, 0, sizeof(res));
    handle_customer_withdraw(0, &req, &res);
    assert(res.status_code == STATUS_SUCCESS);
    assert(res.payload.account.balance == 1300.00);
    printf("[PASS] Withdrawal verified (New Balance: $%.2f)\n", res.payload.account.balance);

    // 5. Test Insufficient Funds (Withdraw $5000)
    memset(&req, 0, sizeof(req));
    req.opcode = OP_WITHDRAW;
    req.user_id = cust.user_id;
    req.role = cust.role;
    req.payload.withdraw.amount = 5000.00;

    memset(&res, 0, sizeof(res));
    handle_customer_withdraw(0, &req, &res);
    assert(res.status_code == STATUS_INSUFFICIENT_FUNDS);
    printf("[PASS] Insufficient funds check verified (Rejection: %s)\n", res.message);

    // 6. Verify Transaction Records Logged
    TransactionRecord txns[10];
    int txn_count = 0;
    assert(db_get_transactions_by_account_id(acc.account_id, txns, 10, &txn_count) == 0);
    assert(txn_count == 2); // Deposit + Withdraw
    printf("[PASS] Transaction logging verified (%d transactions recorded)\n", txn_count);

    printf("=== ALL CUSTOMER OPERATIONS UNIT TESTS PASSED ===\n");
    return 0;
}
