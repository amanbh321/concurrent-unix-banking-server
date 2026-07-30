#ifndef COMMON_H
#define COMMON_H

#define DEFAULT_PORT 8080
#define DATA_DIR "data"
#define METADATA_FILE "data/metadata.dat"
#define USERS_FILE "data/users.dat"
#define ACCOUNTS_FILE "data/accounts.dat"
#define TRANSACTIONS_FILE "data/transactions.dat"
#define LOANS_FILE "data/loans.dat"
#define FEEDBACK_FILE "data/feedback.dat"

#define MAX_SESSIONS 128
#define MAX_RECORDS_PER_RESPONSE 20

// Opcodes (Client -> Server)
#define OP_LOGIN                1
#define OP_LOGOUT               2
#define OP_EXIT                 3

// Customer Opcodes
#define OP_VIEW_BALANCE         10
#define OP_DEPOSIT              11
#define OP_WITHDRAW             12
#define OP_TRANSFER             13
#define OP_APPLY_LOAN           14
#define OP_VIEW_TRANSACTIONS    15
#define OP_ADD_FEEDBACK         16
#define OP_CHANGE_PASSWORD      17

// Employee Opcodes
#define OP_ADD_CUSTOMER         20
#define OP_MODIFY_CUSTOMER      21
#define OP_VIEW_ASSIGNED_LOANS  22
#define OP_PROCESS_LOAN         23
#define OP_VIEW_CUST_TXNS       24

// Manager Opcodes
#define OP_ACTIVATE_ACCOUNT     30
#define OP_DEACTIVATE_ACCOUNT   31
#define OP_ASSIGN_LOAN          32
#define OP_REVIEW_FEEDBACK      33

// Admin Opcodes
#define OP_ADD_EMPLOYEE         40
#define OP_MODIFY_USER          41
#define OP_MANAGE_ROLE          42

// Status Codes (Server -> Client)
#define STATUS_SUCCESS              0
#define STATUS_FAILURE             -1
#define STATUS_AUTH_FAILED         -2
#define STATUS_ALREADY_LOGGED_IN   -3
#define STATUS_UNAUTHORIZED        -4
#define STATUS_ACCOUNT_INACTIVE    -5
#define STATUS_INSUFFICIENT_FUNDS  -6
#define STATUS_NOT_FOUND           -7
#define STATUS_INVALID_INPUT       -8

#endif // COMMON_H
