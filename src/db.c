#include "db.h"
#include "common.h"
#include "models.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

// --- fcntl record locking ---

int db_lock_record(int fd, off_t offset, size_t size, int lock_type) {
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = size;

    if (lock_type == LOCK_READ)        fl.l_type = F_RDLCK;
    else if (lock_type == LOCK_WRITE)  fl.l_type = F_WRLCK;
    else if (lock_type == LOCK_UNLOCK) fl.l_type = F_UNLCK;
    else return -1;

    return fcntl(fd, F_SETLKW, &fl);
}

// --- Generic record operations (used by all entity functions below) ---

// Append a record to end of file with write lock + fsync
static int append_record(const char *filepath, const void *record, size_t rec_size) {
    int fd = open(filepath, O_WRONLY | O_APPEND);
    if (fd == -1) return -1;

    off_t offset = lseek(fd, 0, SEEK_END);
    db_lock_record(fd, offset, rec_size, LOCK_WRITE);
    ssize_t n = write(fd, record, rec_size);
    fsync(fd);
    db_lock_record(fd, offset, rec_size, LOCK_UNLOCK);
    close(fd);
    return (n == (ssize_t)rec_size) ? 0 : -1;
}

// Find first record where the int at key_offset matches key_val
static int find_by_int(const char *filepath, size_t rec_size, size_t key_offset, int key_val, void *out) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return -1;

    char buf[rec_size];
    off_t offset = 0;
    while (1) {
        db_lock_record(fd, offset, rec_size, LOCK_READ);
        ssize_t n = read(fd, buf, rec_size);
        db_lock_record(fd, offset, rec_size, LOCK_UNLOCK);
        if (n != (ssize_t)rec_size) break;

        if (*(int *)(buf + key_offset) == key_val) {
            memcpy(out, buf, rec_size);
            close(fd);
            return 0;
        }
        offset += rec_size;
    }
    close(fd);
    return -1;
}

// Find first record where the string at str_offset matches str_val
static int find_by_str(const char *filepath, size_t rec_size, size_t str_offset, const char *str_val, void *out) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return -1;

    char buf[rec_size];
    off_t offset = 0;
    while (1) {
        db_lock_record(fd, offset, rec_size, LOCK_READ);
        ssize_t n = read(fd, buf, rec_size);
        db_lock_record(fd, offset, rec_size, LOCK_UNLOCK);
        if (n != (ssize_t)rec_size) break;

        if (strcmp((char *)(buf + str_offset), str_val) == 0) {
            memcpy(out, buf, rec_size);
            close(fd);
            return 0;
        }
        offset += rec_size;
    }
    close(fd);
    return -1;
}

// Update in-place: find record by int key, overwrite with new data
static int update_by_int(const char *filepath, size_t rec_size, size_t key_offset, int key_val, const void *record) {
    int fd = open(filepath, O_RDWR);
    if (fd == -1) return -1;

    char buf[rec_size];
    off_t offset = 0;
    while (read(fd, buf, rec_size) == (ssize_t)rec_size) {
        if (*(int *)(buf + key_offset) == key_val) {
            db_lock_record(fd, offset, rec_size, LOCK_WRITE);
            lseek(fd, offset, SEEK_SET);
            ssize_t n = write(fd, record, rec_size);
            fsync(fd);
            db_lock_record(fd, offset, rec_size, LOCK_UNLOCK);
            close(fd);
            return (n == (ssize_t)rec_size) ? 0 : -1;
        }
        offset += rec_size;
    }
    close(fd);
    return -1;
}

// Scan and collect records where int at field_offset matches field_val
static int scan_by_int(const char *filepath, size_t rec_size, size_t field_offset, int field_val, void *out_array, int max_count, int *out_count) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return -1;

    char buf[rec_size];
    off_t offset = 0;
    int count = 0;
    while (count < max_count) {
        db_lock_record(fd, offset, rec_size, LOCK_READ);
        ssize_t n = read(fd, buf, rec_size);
        db_lock_record(fd, offset, rec_size, LOCK_UNLOCK);
        if (n != (ssize_t)rec_size) break;

        if (*(int *)(buf + field_offset) == field_val) {
            memcpy((char *)out_array + count * rec_size, buf, rec_size);
            count++;
        }
        offset += rec_size;
    }
    close(fd);
    *out_count = count;
    return 0;
}

// --- Database initialization ---

int db_init(void) {
    struct stat st;
    if (stat(DATA_DIR, &st) == -1)
        if (mkdir(DATA_DIR, 0755) == -1) return -1;

    // Bootstrap metadata.dat
    if (stat(METADATA_FILE, &st) == -1) {
        int fd = open(METADATA_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd != -1) {
            MetadataRecord meta = { 1001, 5001, 1, 1, 1 };
            db_lock_record(fd, 0, sizeof(meta), LOCK_WRITE);
            write(fd, &meta, sizeof(meta));
            fsync(fd);
            db_lock_record(fd, 0, sizeof(meta), LOCK_UNLOCK);
            close(fd);
        }
    }

    // Bootstrap admin user
    if (stat(USERS_FILE, &st) == -1) {
        int fd = open(USERS_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd != -1) {
            UserRecord admin;
            memset(&admin, 0, sizeof(admin));
            admin.user_id = 1000;
            strncpy(admin.username, "admin", sizeof(admin.username) - 1);
            strncpy(admin.password, "admin123", sizeof(admin.password) - 1);
            admin.role = ROLE_ADMIN;
            admin.is_active = 1;
            strncpy(admin.full_name, "System Administrator", sizeof(admin.full_name) - 1);
            strncpy(admin.gender, "N/A", sizeof(admin.gender) - 1);
            strncpy(admin.phone, "0000000000", sizeof(admin.phone) - 1);
            strncpy(admin.email, "admin@bank.com", sizeof(admin.email) - 1);
            strncpy(admin.address, "Headquarters", sizeof(admin.address) - 1);

            write(fd, &admin, sizeof(admin));
            fsync(fd);
            close(fd);
        }
    }

    // Create remaining data files if missing
    const char *files[] = { ACCOUNTS_FILE, TRANSACTIONS_FILE, LOANS_FILE, FEEDBACK_FILE };
    for (int i = 0; i < 4; i++) {
        if (stat(files[i], &st) == -1) {
            int fd = open(files[i], O_WRONLY | O_CREAT | O_EXCL, 0644);
            if (fd != -1) close(fd);
        }
    }
    return 0;
}

// --- Atomic ID generation ---

static int get_next_id_field(size_t field_offset) {
    int fd = open(METADATA_FILE, O_RDWR);
    if (fd == -1) return -1;

    db_lock_record(fd, 0, sizeof(MetadataRecord), LOCK_WRITE);
    MetadataRecord meta;
    if (read(fd, &meta, sizeof(meta)) != sizeof(meta)) {
        db_lock_record(fd, 0, sizeof(meta), LOCK_UNLOCK);
        close(fd);
        return -1;
    }

    int *ptr = (int *)((char *)&meta + field_offset);
    int new_id = (*ptr)++;

    lseek(fd, 0, SEEK_SET);
    write(fd, &meta, sizeof(meta));
    fsync(fd);
    db_lock_record(fd, 0, sizeof(meta), LOCK_UNLOCK);
    close(fd);
    return new_id;
}

int db_get_next_user_id(void)        { return get_next_id_field(offsetof(MetadataRecord, next_user_id)); }
int db_get_next_account_id(void)     { return get_next_id_field(offsetof(MetadataRecord, next_account_id)); }
int db_get_next_transaction_id(void) { return get_next_id_field(offsetof(MetadataRecord, next_transaction_id)); }
int db_get_next_loan_id(void)        { return get_next_id_field(offsetof(MetadataRecord, next_loan_id)); }
int db_get_next_feedback_id(void)    { return get_next_id_field(offsetof(MetadataRecord, next_feedback_id)); }

// --- User CRUD (thin wrappers over generic functions) ---

int db_write_user(const UserRecord *u)                     { return append_record(USERS_FILE, u, sizeof(UserRecord)); }
int db_update_user(const UserRecord *u)                    { return update_by_int(USERS_FILE, sizeof(UserRecord), offsetof(UserRecord, user_id), u->user_id, u); }
int db_find_user_by_id(int id, UserRecord *out)            { return find_by_int(USERS_FILE, sizeof(UserRecord), offsetof(UserRecord, user_id), id, out); }
int db_find_user_by_username(const char *name, UserRecord *out) { return find_by_str(USERS_FILE, sizeof(UserRecord), offsetof(UserRecord, username), name, out); }

// --- Account CRUD ---

int db_write_account(const AccountRecord *a)                         { return append_record(ACCOUNTS_FILE, a, sizeof(AccountRecord)); }
int db_update_account(const AccountRecord *a)                        { return update_by_int(ACCOUNTS_FILE, sizeof(AccountRecord), offsetof(AccountRecord, account_id), a->account_id, a); }
int db_find_account_id(int id, AccountRecord *out)                   { return find_by_int(ACCOUNTS_FILE, sizeof(AccountRecord), offsetof(AccountRecord, account_id), id, out); }
int db_find_account_by_customer_id(int cid, AccountRecord *out)      { return find_by_int(ACCOUNTS_FILE, sizeof(AccountRecord), offsetof(AccountRecord, customer_id), cid, out); }

// --- Transaction operations ---

int db_write_transaction(const TransactionRecord *t) { return append_record(TRANSACTIONS_FILE, t, sizeof(TransactionRecord)); }

int db_get_transactions_by_account_id(int account_id, TransactionRecord *out_txns, int max_count, int *out_count) {
    int fd = open(TRANSACTIONS_FILE, O_RDONLY);
    if (fd == -1) return -1;

    TransactionRecord t;
    off_t offset = 0;
    int count = 0;
    while (count < max_count) {
        db_lock_record(fd, offset, sizeof(t), LOCK_READ);
        ssize_t n = read(fd, &t, sizeof(t));
        db_lock_record(fd, offset, sizeof(t), LOCK_UNLOCK);
        if (n != sizeof(t)) break;

        if (t.source_account_id == account_id || t.destination_account_id == account_id)
            out_txns[count++] = t;
        offset += sizeof(t);
    }
    close(fd);
    *out_count = count;
    return 0;
}

// --- Loan CRUD ---

int db_write_loan(const LoanRecord *l)     { return append_record(LOANS_FILE, l, sizeof(LoanRecord)); }
int db_update_loan(const LoanRecord *l)    { return update_by_int(LOANS_FILE, sizeof(LoanRecord), offsetof(LoanRecord, loan_id), l->loan_id, l); }
int db_find_loan_by_id(int id, LoanRecord *out) { return find_by_int(LOANS_FILE, sizeof(LoanRecord), offsetof(LoanRecord, loan_id), id, out); }

int db_get_loans_by_customer_id(int cid, LoanRecord *out, int max, int *cnt)       { return scan_by_int(LOANS_FILE, sizeof(LoanRecord), offsetof(LoanRecord, customer_id), cid, out, max, cnt); }
int db_get_loans_by_assigned_employee(int eid, LoanRecord *out, int max, int *cnt)  { return scan_by_int(LOANS_FILE, sizeof(LoanRecord), offsetof(LoanRecord, assigned_employee_id), eid, out, max, cnt); }
int db_get_all_pending_loans(LoanRecord *out, int max, int *cnt)                    { return scan_by_int(LOANS_FILE, sizeof(LoanRecord), offsetof(LoanRecord, status), LOAN_PENDING, out, max, cnt); }

// --- Feedback CRUD ---

int db_write_feedback(const FeedbackRecord *f)  { return append_record(FEEDBACK_FILE, f, sizeof(FeedbackRecord)); }
int db_update_feedback(const FeedbackRecord *f) { return update_by_int(FEEDBACK_FILE, sizeof(FeedbackRecord), offsetof(FeedbackRecord, feedback_id), f->feedback_id, f); }

int db_get_all_feedback(FeedbackRecord *out_fbs, int max_count, int *out_count) {
    int fd = open(FEEDBACK_FILE, O_RDONLY);
    if (fd == -1) return -1;

    FeedbackRecord f;
    off_t offset = 0;
    int count = 0;
    while (count < max_count) {
        db_lock_record(fd, offset, sizeof(f), LOCK_READ);
        ssize_t n = read(fd, &f, sizeof(f));
        db_lock_record(fd, offset, sizeof(f), LOCK_UNLOCK);
        if (n != sizeof(f)) break;
        out_fbs[count++] = f;
        offset += sizeof(f);
    }
    close(fd);
    *out_count = count;
    return 0;
}
