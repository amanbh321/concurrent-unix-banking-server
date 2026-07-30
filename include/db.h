#ifndef DB_H
#define DB_H

#include "models.h"
#include <stddef.h>
#include <sys/types.h>

// Lock types for fcntl locking
#define LOCK_READ  1
#define LOCK_WRITE 2
#define LOCK_UNLOCK 3

// Database Engine Initialization
int db_init(void);

// Atomic ID Generation (via metadata.dat with fcntl write locking & fsync)
int db_get_next_user_id(void);
int db_get_next_account_id(void);
int db_get_next_transaction_id(void);
int db_get_next_loan_id(void);
int db_get_next_feedback_id(void);

// Generic fcntl Record Locking helper
int db_lock_record(int fd, off_t offset, size_t size, int lock_type);

// Entity CRUD & Scan Primitives (returns 0 on success, -1 on error)
int db_write_user(const UserRecord *user);
int db_update_user(const UserRecord *user);
int db_find_user_by_id(int user_id, UserRecord *out_user);
int db_find_user_by_username(const char *username, UserRecord *out_user);

int db_write_account(const AccountRecord *account);
int db_update_account(const AccountRecord *account);
int db_find_account_id(int account_id, AccountRecord *out_account);
int db_find_account_by_customer_id(int customer_id, AccountRecord *out_account);

int db_write_transaction(const TransactionRecord *txn);
int db_get_transactions_by_account_id(int account_id, TransactionRecord *out_txns, int max_count, int *out_count);

int db_write_loan(const LoanRecord *loan);
int db_update_loan(const LoanRecord *loan);
int db_find_loan_by_id(int loan_id, LoanRecord *out_loan);
int db_get_loans_by_customer_id(int customer_id, LoanRecord *out_loans, int max_count, int *out_count);
int db_get_loans_by_assigned_employee(int employee_id, LoanRecord *out_loans, int max_count, int *out_count);
int db_get_all_pending_loans(LoanRecord *out_loans, int max_count, int *out_count);

int db_write_feedback(const FeedbackRecord *fb);
int db_update_feedback(const FeedbackRecord *fb);
int db_get_all_feedback(FeedbackRecord *out_feedbacks, int max_count, int *out_count);

#endif // DB_H
