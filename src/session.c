#include "session.h"
#include "common.h"
#include <string.h>
#include <pthread.h>

static SessionEntry active_sessions[MAX_SESSIONS];
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

void session_init(void) {
    pthread_mutex_lock(&session_mutex);
    memset(active_sessions, 0, sizeof(active_sessions));
    pthread_mutex_unlock(&session_mutex);
}

int session_add(int user_id, int client_fd, UserRole role) {
    pthread_mutex_lock(&session_mutex);

    // 1. Check if user is already logged in
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i].user_id == user_id) {
            pthread_mutex_unlock(&session_mutex);
            return STATUS_ALREADY_LOGGED_IN;
        }
    }

    // 2. Find free slot
    int free_idx = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i].user_id == 0) {
            free_idx = i;
            break;
        }
    }

    if (free_idx == -1) {
        pthread_mutex_unlock(&session_mutex);
        return -1; // Table full
    }

    // 3. Register session
    active_sessions[free_idx].user_id = user_id;
    active_sessions[free_idx].client_fd = client_fd;
    active_sessions[free_idx].role = role;

    pthread_mutex_unlock(&session_mutex);
    return 0;
}

void session_remove_by_fd(int client_fd) {
    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i].client_fd == client_fd) {
            memset(&active_sessions[i], 0, sizeof(SessionEntry));
        }
    }
    pthread_mutex_unlock(&session_mutex);
}

void session_remove_by_user_id(int user_id) {
    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i].user_id == user_id) {
            memset(&active_sessions[i], 0, sizeof(SessionEntry));
        }
    }
    pthread_mutex_unlock(&session_mutex);
}

int session_is_active(int user_id) {
    pthread_mutex_lock(&session_mutex);
    int active = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i].user_id == user_id) {
            active = 1;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
    return active;
}

int session_get_info(int client_fd, int *out_user_id, UserRole *out_role) {
    pthread_mutex_lock(&session_mutex);
    int found = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i].client_fd == client_fd && active_sessions[i].user_id != 0) {
            if (out_user_id) *out_user_id = active_sessions[i].user_id;
            if (out_role) *out_role = active_sessions[i].role;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
    return found ? 0 : -1;
}
