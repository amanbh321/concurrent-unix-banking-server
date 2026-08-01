#ifndef SESSION_H
#define SESSION_H

#include "models.h"
#include "common.h"

typedef struct {
    int user_id;        // 0 = slot empty
    int client_fd;      // Socket descriptor
    UserRole role;      // Cached user role
} SessionEntry;

// Initialize session tracking
void session_init(void);

// Add active session (returns 0 on success, STATUS_ALREADY_LOGGED_IN if user logged in, -1 on table full)
int session_add(int user_id, int client_fd, UserRole role);

// Remove session by client socket descriptor
void session_remove_by_fd(int client_fd);

// Remove session by user ID
void session_remove_by_user_id(int user_id);

// Check if user ID has an active session
int session_is_active(int user_id);

// Get cached session details for an active client descriptor (returns 0 if found, -1 if not logged in)
int session_get_info(int client_fd, int *out_user_id, UserRole *out_role);

#endif // SESSION_H
