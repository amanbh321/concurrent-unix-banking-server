#include "session.h"
#include "common.h"
#include "models.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    printf("=== Starting Milestone 3 Session Unit Tests ===\n");

    session_init();

    // 1. Add session
    assert(session_add(1001, 4, ROLE_CUSTOMER) == 0);
    assert(session_is_active(1001) == 1);

    // 2. Single session check: duplicate user login attempt
    assert(session_add(1001, 5, ROLE_CUSTOMER) == STATUS_ALREADY_LOGGED_IN);

    // 3. Different user login
    assert(session_add(1002, 5, ROLE_EMPLOYEE) == 0);
    assert(session_is_active(1002) == 1);

    // 4. Session info lookup
    int uid;
    UserRole r;
    assert(session_get_info(4, &uid, &r) == 0);
    assert(uid == 1001);
    assert(r == ROLE_CUSTOMER);

    // 5. Remove session by FD
    session_remove_by_fd(4);
    assert(session_is_active(1001) == 0);
    assert(session_is_active(1002) == 1);

    // 6. Remove session by User ID
    session_remove_by_user_id(1002);
    assert(session_is_active(1002) == 0);

    printf("=== ALL MILESTONE 3 SESSION UNIT TESTS PASSED ===\n");
    return 0;
}
