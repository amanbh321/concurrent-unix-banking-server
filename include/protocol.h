#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "models.h"
#include "common.h"

typedef struct {
    int opcode;
    int user_id;              // Populated after login
    UserRole role;            // Populated after login
    union {
        struct { char username[32]; char password[64]; } login;
        struct { double amount; } deposit;
        struct { double amount; } withdraw;
        struct { int target_account_id; double amount; } transfer;
        struct { double amount; } loan_apply;
        struct { int loan_id; int approve; char remarks[128]; } loan_process;
        struct { int loan_id; int employee_id; } loan_assign;
        struct { int account_id; } account_toggle;
        struct { char feedback_text[256]; } feedback;
        struct { char old_password[64]; char new_password[64]; } change_pass;
        struct {
            char username[32]; char password[64];
            char full_name[64]; int age; char gender[8];
            char phone[16]; char email[48]; char address[128];
            double initial_deposit;
        } create_user;
        struct {
            int target_user_id;
            char full_name[64]; char phone[16]; char email[48]; char address[128];
        } modify_user;
        struct { int target_user_id; UserRole new_role; } manage_role;
        struct { int account_id; } view_txns;
        struct { int feedback_id; } review_feedback;
    } payload;
} RequestPacket;

typedef struct {
    int  status_code;
    char message[256];
    int  record_count;
    union {
        UserRecord        user;
        AccountRecord     account;
        TransactionRecord transactions[MAX_RECORDS_PER_RESPONSE];
        LoanRecord        loans[MAX_RECORDS_PER_RESPONSE];
        FeedbackRecord    feedbacks[MAX_RECORDS_PER_RESPONSE];
    } payload;
} ResponsePacket;

#endif // PROTOCOL_H
