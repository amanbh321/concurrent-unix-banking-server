#include "db.h"
#include "common.h"
#include "models.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

// Lock helper using fcntl
int db_lock_record(int fd, off_t offset, size_t size, int lock_type) {
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = size;

    if (lock_type == LOCK_READ) {
        fl.l_type = F_RDLCK;
    } else if (lock_type == LOCK_WRITE) {
        fl.l_type = F_WRLCK;
    } else if (lock_type == LOCK_UNLOCK) {
        fl.l_type = F_UNLCK;
    } else {
        return -1;
    }

    return fcntl(fd, F_SETLKW, &fl);
}

// Database initialization & file bootstrapping
int db_init(void) {
    // Create data directory if missing
    struct stat st;
    if (stat(DATA_DIR, &st) == -1) {
        if (mkdir(DATA_DIR, 0755) == -1) {
            return -1;
        }
    }

    // 1. Initialize metadata.dat if absent
    if (stat(METADATA_FILE, &st) == -1) {
        int fd = open(METADATA_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd != -1) {
            MetadataRecord meta;
            meta.next_user_id = 1001;
            meta.next_account_id = 5001;
            meta.next_transaction_id = 1;
            meta.next_loan_id = 1;
            meta.next_feedback_id = 1;

            db_lock_record(fd, 0, sizeof(MetadataRecord), LOCK_WRITE);
            write(fd, &meta, sizeof(MetadataRecord));
            fsync(fd);
            db_lock_record(fd, 0, sizeof(MetadataRecord), LOCK_UNLOCK);
            close(fd);
        }
    }

    // 2. Initialize users.dat and bootstrap Admin (ID 1000) if absent
    if (stat(USERS_FILE, &st) == -1) {
        int fd = open(USERS_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd != -1) {
            UserRecord admin;
            memset(&admin, 0, sizeof(admin));
            admin.user_id = 1000;
            strncpy(admin.username, "admin", sizeof(admin.username) - 1);
            strncpy(admin.password, "admin123", sizeof(admin.password) - 1);
            admin.role = ROLE_ADMIN;
            admin.account_id = 0;
            admin.age = 35;
            admin.is_active = 1;
            strncpy(admin.full_name, "System Administrator", sizeof(admin.full_name) - 1);
            strncpy(admin.gender, "N/A", sizeof(admin.gender) - 1);
            strncpy(admin.phone, "0000000000", sizeof(admin.phone) - 1);
            strncpy(admin.email, "admin@bank.com", sizeof(admin.email) - 1);
            strncpy(admin.address, "Headquarters", sizeof(admin.address) - 1);

            db_lock_record(fd, 0, sizeof(UserRecord), LOCK_WRITE);
            write(fd, &admin, sizeof(UserRecord));
            fsync(fd);
            db_lock_record(fd, 0, sizeof(UserRecord), LOCK_UNLOCK);
            close(fd);
        }
    }

    // 3. Ensure remaining data files exist
    const char *files[] = { ACCOUNTS_FILE, TRANSACTIONS_FILE, LOANS_FILE, FEEDBACK_FILE };
    for (int i = 0; i < 4; i++) {
        if (stat(files[i], &st) == -1) {
            int fd = open(files[i], O_WRONLY | O_CREAT | O_EXCL, 0644);
            if (fd != -1) {
                close(fd);
            }
        }
    }

    return 0;
}

// Atomic ID allocation primitive
static int get_next_id_field(size_t field_offset) {
    int fd = open(METADATA_FILE, O_RDWR);
    if (fd == -1) return -1;

    if (db_lock_record(fd, 0, sizeof(MetadataRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    MetadataRecord meta;
    if (read(fd, &meta, sizeof(MetadataRecord)) != sizeof(MetadataRecord)) {
        db_lock_record(fd, 0, sizeof(MetadataRecord), LOCK_UNLOCK);
        close(fd);
        return -1;
    }

    int *ptr = (int *)((char *)&meta + field_offset);
    int new_id = *ptr;
    (*ptr)++;

    lseek(fd, 0, SEEK_SET);
    write(fd, &meta, sizeof(MetadataRecord));
    fsync(fd);

    db_lock_record(fd, 0, sizeof(MetadataRecord), LOCK_UNLOCK);
    close(fd);

    return new_id;
}

int db_get_next_user_id(void) {
    return get_next_id_field(offsetof(MetadataRecord, next_user_id));
}

int db_get_next_account_id(void) {
    return get_next_id_field(offsetof(MetadataRecord, next_account_id));
}

int db_get_next_transaction_id(void) {
    return get_next_id_field(offsetof(MetadataRecord, next_transaction_id));
}

int db_get_next_loan_id(void) {
    return get_next_id_field(offsetof(MetadataRecord, next_loan_id));
}

int db_get_next_feedback_id(void) {
    return get_next_id_field(offsetof(MetadataRecord, next_feedback_id));
}

// --- USER OPERATIONS ---

int db_write_user(const UserRecord *user) {
    int fd = open(USERS_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) return -1;

    off_t offset = lseek(fd, 0, SEEK_END);
    if (db_lock_record(fd, offset, sizeof(UserRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    ssize_t bytes = write(fd, user, sizeof(UserRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(UserRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(UserRecord)) ? 0 : -1;
}

int db_update_user(const UserRecord *user) {
    int fd = open(USERS_FILE, O_RDWR);
    if (fd == -1) return -1;

    UserRecord current;
    off_t offset = 0;
    int found = 0;

    while (read(fd, &current, sizeof(UserRecord)) == sizeof(UserRecord)) {
        if (current.user_id == user->user_id) {
            found = 1;
            break;
        }
        offset += sizeof(UserRecord);
    }

    if (!found) {
        close(fd);
        return -1;
    }

    if (db_lock_record(fd, offset, sizeof(UserRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    lseek(fd, offset, SEEK_SET);
    ssize_t bytes = write(fd, user, sizeof(UserRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(UserRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(UserRecord)) ? 0 : -1;
}

int db_find_user_by_id(int user_id, UserRecord *out_user) {
    int fd = open(USERS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    UserRecord current;
    off_t offset = 0;
    int found = 0;

    while (1) {
        if (db_lock_record(fd, offset, sizeof(UserRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(UserRecord));
        db_lock_record(fd, offset, sizeof(UserRecord), LOCK_UNLOCK);

        if (bytes != sizeof(UserRecord)) break;

        if (current.user_id == user_id) {
            *out_user = current;
            found = 1;
            break;
        }
        offset += sizeof(UserRecord);
    }

    close(fd);
    return found ? 0 : -1;
}

int db_find_user_by_username(const char *username, UserRecord *out_user) {
    int fd = open(USERS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    UserRecord current;
    off_t offset = 0;
    int found = 0;

    while (1) {
        if (db_lock_record(fd, offset, sizeof(UserRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(UserRecord));
        db_lock_record(fd, offset, sizeof(UserRecord), LOCK_UNLOCK);

        if (bytes != sizeof(UserRecord)) break;

        if (strcmp(current.username, username) == 0) {
            *out_user = current;
            found = 1;
            break;
        }
        offset += sizeof(UserRecord);
    }

    close(fd);
    return found ? 0 : -1;
}

// --- ACCOUNT OPERATIONS ---

int db_write_account(const AccountRecord *account) {
    int fd = open(ACCOUNTS_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) return -1;

    off_t offset = lseek(fd, 0, SEEK_END);
    if (db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    ssize_t bytes = write(fd, account, sizeof(AccountRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(AccountRecord)) ? 0 : -1;
}

int db_update_account(const AccountRecord *account) {
    int fd = open(ACCOUNTS_FILE, O_RDWR);
    if (fd == -1) return -1;

    AccountRecord current;
    off_t offset = 0;
    int found = 0;

    while (read(fd, &current, sizeof(AccountRecord)) == sizeof(AccountRecord)) {
        if (current.account_id == account->account_id) {
            found = 1;
            break;
        }
        offset += sizeof(AccountRecord);
    }

    if (!found) {
        close(fd);
        return -1;
    }

    if (db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    lseek(fd, offset, SEEK_SET);
    ssize_t bytes = write(fd, account, sizeof(AccountRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(AccountRecord)) ? 0 : -1;
}

int db_find_account_id(int account_id, AccountRecord *out_account) {
    int fd = open(ACCOUNTS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    AccountRecord current;
    off_t offset = 0;
    int found = 0;

    while (1) {
        if (db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(AccountRecord));
        db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_UNLOCK);

        if (bytes != sizeof(AccountRecord)) break;

        if (current.account_id == account_id) {
            *out_account = current;
            found = 1;
            break;
        }
        offset += sizeof(AccountRecord);
    }

    close(fd);
    return found ? 0 : -1;
}

int db_find_account_by_customer_id(int customer_id, AccountRecord *out_account) {
    int fd = open(ACCOUNTS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    AccountRecord current;
    off_t offset = 0;
    int found = 0;

    while (1) {
        if (db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(AccountRecord));
        db_lock_record(fd, offset, sizeof(AccountRecord), LOCK_UNLOCK);

        if (bytes != sizeof(AccountRecord)) break;

        if (current.customer_id == customer_id) {
            *out_account = current;
            found = 1;
            break;
        }
        offset += sizeof(AccountRecord);
    }

    close(fd);
    return found ? 0 : -1;
}

// --- TRANSACTION OPERATIONS ---

int db_write_transaction(const TransactionRecord *txn) {
    int fd = open(TRANSACTIONS_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) return -1;

    off_t offset = lseek(fd, 0, SEEK_END);
    if (db_lock_record(fd, offset, sizeof(TransactionRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    ssize_t bytes = write(fd, txn, sizeof(TransactionRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(TransactionRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(TransactionRecord)) ? 0 : -1;
}

int db_get_transactions_by_account_id(int account_id, TransactionRecord *out_txns, int max_count, int *out_count) {
    int fd = open(TRANSACTIONS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    TransactionRecord current;
    off_t offset = 0;
    int count = 0;

    while (count < max_count) {
        if (db_lock_record(fd, offset, sizeof(TransactionRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(TransactionRecord));
        db_lock_record(fd, offset, sizeof(TransactionRecord), LOCK_UNLOCK);

        if (bytes != sizeof(TransactionRecord)) break;

        if (current.source_account_id == account_id || current.destination_account_id == account_id) {
            out_txns[count++] = current;
        }
        offset += sizeof(TransactionRecord);
    }

    close(fd);
    *out_count = count;
    return 0;
}

// --- LOAN OPERATIONS ---

int db_write_loan(const LoanRecord *loan) {
    int fd = open(LOANS_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) return -1;

    off_t offset = lseek(fd, 0, SEEK_END);
    if (db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    ssize_t bytes = write(fd, loan, sizeof(LoanRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(LoanRecord)) ? 0 : -1;
}

int db_update_loan(const LoanRecord *loan) {
    int fd = open(LOANS_FILE, O_RDWR);
    if (fd == -1) return -1;

    LoanRecord current;
    off_t offset = 0;
    int found = 0;

    while (read(fd, &current, sizeof(LoanRecord)) == sizeof(LoanRecord)) {
        if (current.loan_id == loan->loan_id) {
            found = 1;
            break;
        }
        offset += sizeof(LoanRecord);
    }

    if (!found) {
        close(fd);
        return -1;
    }

    if (db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    lseek(fd, offset, SEEK_SET);
    ssize_t bytes = write(fd, loan, sizeof(LoanRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(LoanRecord)) ? 0 : -1;
}

int db_find_loan_by_id(int loan_id, LoanRecord *out_loan) {
    int fd = open(LOANS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    LoanRecord current;
    off_t offset = 0;
    int found = 0;

    while (1) {
        if (db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(LoanRecord));
        db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_UNLOCK);

        if (bytes != sizeof(LoanRecord)) break;

        if (current.loan_id == loan_id) {
            *out_loan = current;
            found = 1;
            break;
        }
        offset += sizeof(LoanRecord);
    }

    close(fd);
    return found ? 0 : -1;
}

int db_get_loans_by_customer_id(int customer_id, LoanRecord *out_loans, int max_count, int *out_count) {
    int fd = open(LOANS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    LoanRecord current;
    off_t offset = 0;
    int count = 0;

    while (count < max_count) {
        if (db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(LoanRecord));
        db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_UNLOCK);

        if (bytes != sizeof(LoanRecord)) break;

        if (current.customer_id == customer_id) {
            out_loans[count++] = current;
        }
        offset += sizeof(LoanRecord);
    }

    close(fd);
    *out_count = count;
    return 0;
}

int db_get_loans_by_assigned_employee(int employee_id, LoanRecord *out_loans, int max_count, int *out_count) {
    int fd = open(LOANS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    LoanRecord current;
    off_t offset = 0;
    int count = 0;

    while (count < max_count) {
        if (db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(LoanRecord));
        db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_UNLOCK);

        if (bytes != sizeof(LoanRecord)) break;

        if (current.assigned_employee_id == employee_id) {
            out_loans[count++] = current;
        }
        offset += sizeof(LoanRecord);
    }

    close(fd);
    *out_count = count;
    return 0;
}

int db_get_all_pending_loans(LoanRecord *out_loans, int max_count, int *out_count) {
    int fd = open(LOANS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    LoanRecord current;
    off_t offset = 0;
    int count = 0;

    while (count < max_count) {
        if (db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(LoanRecord));
        db_lock_record(fd, offset, sizeof(LoanRecord), LOCK_UNLOCK);

        if (bytes != sizeof(LoanRecord)) break;

        if (current.status == LOAN_PENDING) {
            out_loans[count++] = current;
        }
        offset += sizeof(LoanRecord);
    }

    close(fd);
    *out_count = count;
    return 0;
}

// --- FEEDBACK OPERATIONS ---

int db_write_feedback(const FeedbackRecord *fb) {
    int fd = open(FEEDBACK_FILE, O_WRONLY | O_APPEND);
    if (fd == -1) return -1;

    off_t offset = lseek(fd, 0, SEEK_END);
    if (db_lock_record(fd, offset, sizeof(FeedbackRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    ssize_t bytes = write(fd, fb, sizeof(FeedbackRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(FeedbackRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(FeedbackRecord)) ? 0 : -1;
}

int db_update_feedback(const FeedbackRecord *fb) {
    int fd = open(FEEDBACK_FILE, O_RDWR);
    if (fd == -1) return -1;

    FeedbackRecord current;
    off_t offset = 0;
    int found = 0;

    while (read(fd, &current, sizeof(FeedbackRecord)) == sizeof(FeedbackRecord)) {
        if (current.feedback_id == fb->feedback_id) {
            found = 1;
            break;
        }
        offset += sizeof(FeedbackRecord);
    }

    if (!found) {
        close(fd);
        return -1;
    }

    if (db_lock_record(fd, offset, sizeof(FeedbackRecord), LOCK_WRITE) == -1) {
        close(fd);
        return -1;
    }

    lseek(fd, offset, SEEK_SET);
    ssize_t bytes = write(fd, fb, sizeof(FeedbackRecord));
    fsync(fd);
    db_lock_record(fd, offset, sizeof(FeedbackRecord), LOCK_UNLOCK);
    close(fd);

    return (bytes == sizeof(FeedbackRecord)) ? 0 : -1;
}

int db_get_all_feedback(FeedbackRecord *out_feedbacks, int max_count, int *out_count) {
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1) return -1;

    FeedbackRecord current;
    off_t offset = 0;
    int count = 0;

    while (count < max_count) {
        if (db_lock_record(fd, offset, sizeof(FeedbackRecord), LOCK_READ) == -1) {
            break;
        }
        ssize_t bytes = read(fd, &current, sizeof(FeedbackRecord));
        db_lock_record(fd, offset, sizeof(FeedbackRecord), LOCK_UNLOCK);

        if (bytes != sizeof(FeedbackRecord)) break;

        out_feedbacks[count++] = current;
        offset += sizeof(FeedbackRecord);
    }

    close(fd);
    *out_count = count;
    return 0;
}
