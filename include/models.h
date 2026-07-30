#ifndef MODELS_H
#define MODELS_H

typedef enum {
    ROLE_CUSTOMER = 1,
    ROLE_EMPLOYEE = 2,
    ROLE_MANAGER  = 3,
    ROLE_ADMIN    = 4
} UserRole;

typedef enum {
    ACCOUNT_ACTIVE   = 1,
    ACCOUNT_INACTIVE = 0
} AccountStatus;

typedef enum {
    LOAN_PENDING  = 0,
    LOAN_ASSIGNED = 1,
    LOAN_APPROVED = 2,
    LOAN_REJECTED = 3
} LoanStatus;

typedef enum {
    TXN_DEPOSIT      = 1,
    TXN_WITHDRAWAL   = 2,
    TXN_TRANSFER_IN  = 3,
    TXN_TRANSFER_OUT = 4,
    TXN_LOAN_CREDIT  = 5
} TransactionType;

typedef struct {
    int       next_user_id;           // Starts at 1001
    int       next_account_id;        // Starts at 5001
    int       next_transaction_id;    // Starts at 1
    int       next_loan_id;           // Starts at 1
    int       next_feedback_id;       // Starts at 1
} MetadataRecord;

typedef struct {
    int       user_id;            // Primary key (auto-generated)
    char      username[32];       // Unique login username
    char      password[64];       // Plain text password
    UserRole  role;               // CUSTOMER, EMPLOYEE, MANAGER, ADMIN
    int       account_id;         // Linked account ID (customers); 0 for others
    int       age;                // Age
    int       is_active;          // 1 = active, 0 = deactivated
    char      full_name[64];      // Full name
    char      gender[8];          // "Male", "Female", "Other"
    char      phone[16];          // Contact phone
    char      email[48];          // Email address
    char      address[128];       // Residential address
} UserRecord;

typedef struct {
    int            account_id;    // Primary key (auto-generated)
    int            customer_id;   // Foreign key -> UserRecord.user_id
    AccountStatus  status;        // ACTIVE / INACTIVE
    double         balance;       // Current balance
} AccountRecord;

typedef struct {
    int              transaction_id;         // Primary key (auto-generated)
    int              source_account_id;      // Originating account (0 for direct deposits)
    int              destination_account_id; // Destination account (0 for direct withdrawals)
    TransactionType  type;                   // DEPOSIT, WITHDRAWAL, TRANSFER_IN, etc.
    double           amount;                 // Transaction amount
    double           balance_after;          // Balance after transaction
    long             timestamp;              // Epoch timestamp (time_t)
} TransactionRecord;

typedef struct {
    int        loan_id;                  // Primary key (auto-generated)
    int        customer_id;              // Applicant user ID
    int        account_id;               // Account to credit on approval
    LoanStatus status;                   // PENDING, ASSIGNED, APPROVED, REJECTED
    int        assigned_employee_id;     // Assigned Employee ID (0 if unassigned)
    int        processed_by_employee_id; // Processing Employee ID (0 if pending)
    double     amount;                   // Requested loan amount
    long       processed_timestamp;      // Approval/rejection epoch time
    char       remarks[128];             // Evaluator remarks
} LoanRecord;

typedef struct {
    int   feedback_id;          // Primary key (auto-generated)
    int   customer_id;          // Author user ID
    char  message[256];         // Feedback text
    long  timestamp;            // Submission time
    int   reviewed;             // 0 = unreviewed, 1 = reviewed by manager
    long  review_timestamp;     // Manager review epoch time
} FeedbackRecord;

#endif // MODELS_H
