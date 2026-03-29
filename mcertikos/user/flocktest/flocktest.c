/*
 * File Locking System Test Program for mCertiKOS
 *
 * This is a user-space program that tests the flock() system call implementation.
 * Run this in mCertiKOS to validate the file locking functionality.
 */

#include <proc.h>
#include <stdio.h>
#include <file.h>
#include <syscall.h>

#define TEST_FILE "/testfile"

/* Files used to coordinate with the helper child process. */
#define CHILD_MODE_FILE   "/flockchild.mode"
#define CHILD_RESULT_FILE "/flockchild.res"

/* Must match user/flockchild/flockchild.c */
enum {
    CHILD_MODE_TRY_SH_NB = 1,
    CHILD_MODE_TRY_EX_NB = 2,
    CHILD_MODE_ACQ_SH_NB = 3,
    CHILD_MODE_ACQ_EX_NB = 4,
    CHILD_MODE_TRY_SH_EX_NB = 5,
    CHILD_MODE_ACQ_SH_BLOCK = 6,
    CHILD_MODE_ACQ_EX_BLOCK = 7,
    CHILD_MODE_EXIT = 9,
};

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

static void print_separator(void)
{
    printf("----------------------------------------\n");
}

static void test_assert(int condition, const char *message)
{
    test_count++;
    if (condition) {
        pass_count++;
        printf("  [PASS] %s\n", message);
    } else {
        fail_count++;
        printf("  [FAIL] %s\n", message);
    }
}

static int child_nonce = 1;
static int helper_pid = -1;

static int parse_nonce(const char *buf, int n, int start)
{
    int nonce = 0;
    int ok = 0;
    for (int i = start; i < n; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            ok = 1;
            nonce = nonce * 10 + (buf[i] - '0');
        } else if (ok) {
            break;
        }
    }
    return ok ? nonce : -1;
}

static void child_set_mode_with_nonce(int mode, int nonce)
{
    int fd = open(CHILD_MODE_FILE, O_CREATE | O_RDWR);
    if (fd < 0) {
        return;
    }

    /* Write fixed-width 'M<mode>:<nonce8>\n' (11 bytes).
     * Fixed width avoids needing truncate/unlink.
     */
    if (nonce < 0) {
        nonce = 0;
    }
    char buf[12];
    buf[0] = 'M';
    buf[1] = (char)('0' + (mode % 10));
    buf[2] = ':';
    int n = nonce;
    for (int p = 0; p < 8; p++) {
        buf[10 - p] = (char)('0' + (n % 10));
        n /= 10;
    }
    buf[11] = '\n';
    int w = write(fd, buf, 12);
    (void)w;
    close(fd);
}

// Returns:
//   -2 if not ready yet
//   0..9 result code if ready
static int child_poll_result(int nonce)
{
    int fd = open(CHILD_RESULT_FILE, O_RDONLY);
    if (fd < 0) {
        return -2;
    }
    char buf[32];
    int n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n < 3) {
        return -2;
    }
    if (buf[0] != 'R' || buf[1] < '0' || buf[1] > '9') {
        return -2;
    }
    int got = parse_nonce(buf, n, 2);
    if (got != nonce) {
        return -2;
    }
    return (int)(buf[1] - '0');
}

static int child_wait_for_result(int nonce)
{
    for (int attempts = 0; attempts < 400; attempts++) {
        for (int i = 0; i < 5; i++) {
            yield();
        }
        int rc = child_poll_result(nonce);
        if (rc >= 0) {
            return rc;
        }
    }
    return 9;
}

static int helper_start(void)
{
    if (helper_pid != -1) {
        return 0;
    }
    /* Keep coordination files stable; overwrite fixed-width records instead. */
    helper_pid = (int)spawn(7, 32);
    if (helper_pid >= NUM_IDS) {
        helper_pid = -1;
        return -1;
    }
    return 0;
}

static int helper_request(int mode)
{
    if (helper_start() < 0) {
        return -1;
    }
    int nonce = child_nonce++;
    child_set_mode_with_nonce(mode, nonce);
    return child_wait_for_result(nonce);
}

static void helper_stop(void)
{
    if (helper_pid == -1) {
        return;
    }
    int nonce = child_nonce++;
    child_set_mode_with_nonce(CHILD_MODE_EXIT, nonce);
    child_wait_for_result(nonce);
    helper_pid = -1;
}

void test_blocking_exclusive_waits_for_shared_release(void)
{
    printf("\nTest 15: Blocking Exclusive Waits For Shared Release\n");
    print_separator();

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    test_assert(fd >= 0, "File opened successfully");
    if (fd < 0) {
        return;
    }

    int result = flock(fd, LOCK_SH);
    test_assert(result == 0, "Parent acquired shared lock");
    if (result != 0) {
        close(fd);
        return;
    }

    if (helper_start() < 0) {
        test_assert(0, "Spawned child for blocking exclusive");
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }
    test_assert(1, "Spawned child for blocking exclusive");

    int nonce = child_nonce++;
    child_set_mode_with_nonce(CHILD_MODE_ACQ_EX_BLOCK, nonce);

    // Give child time to run and (hopefully) block.
    for (int i = 0; i < 200; i++) {
        yield();
    }

    int early = child_poll_result(nonce);
    test_assert(early == -2, "Child has not completed before parent unlock (blocked)");

    // Now release and ensure child completes.
    flock(fd, LOCK_UN);
    int child_rc = child_wait_for_result(nonce);
    test_assert(child_rc == 0, "Child acquired exclusive after parent released shared");

    close(fd);
}

void test_blocking_shared_waits_for_exclusive_release(void)
{
    printf("\nTest 16: Blocking Shared Waits For Exclusive Release\n");
    print_separator();

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    test_assert(fd >= 0, "File opened successfully");
    if (fd < 0) {
        return;
    }

    int result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Parent acquired exclusive lock");
    if (result != 0) {
        close(fd);
        return;
    }

    if (helper_start() < 0) {
        test_assert(0, "Spawned child for blocking shared");
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }
    test_assert(1, "Spawned child for blocking shared");

    int nonce = child_nonce++;
    child_set_mode_with_nonce(CHILD_MODE_ACQ_SH_BLOCK, nonce);

    for (int i = 0; i < 200; i++) {
        yield();
    }
    int early = child_poll_result(nonce);
    test_assert(early == -2, "Child has not completed before parent unlock (blocked)");

    flock(fd, LOCK_UN);
    int child_rc = child_wait_for_result(nonce);
    test_assert(child_rc == 0, "Child acquired shared after parent released exclusive");

    close(fd);
}

void test_multiprocess_exclusive_blocks_child(void)
{
    printf("\nTest 12: Multi-process Exclusive Blocks Child (Non-blocking)\n");
    print_separator();

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    test_assert(fd >= 0, "File opened successfully");
    if (fd < 0) {
        return;
    }

    int result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Parent acquired exclusive lock");
    if (result != 0) {
        close(fd);
        return;
    }

    int child_rc = helper_request(CHILD_MODE_TRY_SH_EX_NB);
    test_assert(child_rc == 0, "Child shared+exclusive LOCK_NB fail while parent holds exclusive");

    flock(fd, LOCK_UN);
    close(fd);
}

void test_multiprocess_shared_blocks_child_exclusive(void)
{
    printf("\nTest 13: Multi-process Shared Blocks Child Exclusive (Non-blocking)\n");
    print_separator();

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    test_assert(fd >= 0, "File opened successfully");
    if (fd < 0) {
        return;
    }

    int result = flock(fd, LOCK_SH);
    test_assert(result == 0, "Parent acquired shared lock");
    if (result != 0) {
        close(fd);
        return;
    }

    int child_rc = helper_request(CHILD_MODE_TRY_EX_NB);
    test_assert(child_rc == 0, "Child exclusive LOCK_NB fails while parent holds shared");

    flock(fd, LOCK_UN);
    close(fd);
}

void test_multiprocess_child_succeeds_after_release(void)
{
    printf("\nTest 14: Multi-process Child Succeeds After Release (Non-blocking)\n");
    print_separator();

    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    test_assert(fd >= 0, "File opened successfully");
    if (fd < 0) {
        return;
    }

    int result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Parent acquired exclusive lock");
    if (result != 0) {
        close(fd);
        return;
    }

    flock(fd, LOCK_UN);
    close(fd);

    int child_rc = helper_request(CHILD_MODE_ACQ_EX_NB);
    test_assert(child_rc == 0, "Child exclusive LOCK_NB succeeds after parent release");
}

void test_basic_shared_lock()
{
    printf("\nTest 1: Basic Shared Lock\n");
    print_separator();

    // Create a test file
    int fd = open(TEST_FILE, O_CREATE | O_RDWR);
    test_assert(fd >= 0, "File opened successfully");

    if (fd < 0)
    {
        printf("  ERROR: Cannot create test file\n");
        return;
    }

    // Acquire shared lock
    int result = flock(fd, LOCK_SH);
    test_assert(result == 0, "Shared lock acquired");

    // Release lock
    result = flock(fd, LOCK_UN);
    test_assert(result == 0, "Lock released");

    close(fd);
}

void test_basic_exclusive_lock()
{
    printf("\nTest 2: Basic Exclusive Lock\n");
    print_separator();

    int fd = open(TEST_FILE, O_RDWR);
    test_assert(fd >= 0, "File opened successfully");

    if (fd < 0)
        return;

    // Acquire exclusive lock
    int result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Exclusive lock acquired");

    // Release lock
    result = flock(fd, LOCK_UN);
    test_assert(result == 0, "Lock released");

    close(fd);
}

void test_multiple_shared_locks()
{
    printf("\nTest 3: Multiple Shared Locks (Same Process)\n");
    print_separator();

    int fd1 = open(TEST_FILE, O_RDWR);
    int fd2 = open(TEST_FILE, O_RDWR);

    test_assert(fd1 >= 0 && fd2 >= 0, "Both file descriptors opened");

    if (fd1 < 0 || fd2 < 0)
    {
        if (fd1 >= 0)
            close(fd1);
        if (fd2 >= 0)
            close(fd2);
        return;
    }

    // Both should be able to acquire shared locks
    int result1 = flock(fd1, LOCK_SH);
    test_assert(result1 == 0, "First shared lock acquired");

    int result2 = flock(fd2, LOCK_SH);
    test_assert(result2 == 0, "Second shared lock acquired");

    // Release both
    flock(fd1, LOCK_UN);
    flock(fd2, LOCK_UN);

    close(fd1);
    close(fd2);
}

void test_exclusive_blocks_shared()
{
    printf("\nTest 4: Exclusive Lock Blocks Shared (Non-blocking)\n");
    print_separator();

    int fd1 = open(TEST_FILE, O_RDWR);
    int fd2 = open(TEST_FILE, O_RDWR);

    test_assert(fd1 >= 0 && fd2 >= 0, "Both file descriptors opened");

    if (fd1 < 0 || fd2 < 0)
    {
        if (fd1 >= 0)
            close(fd1);
        if (fd2 >= 0)
            close(fd2);
        return;
    }

    // Acquire exclusive lock on fd1
    int result = flock(fd1, LOCK_EX);
    test_assert(result == 0, "Exclusive lock acquired on fd1");

    // Try non-blocking shared lock on fd2 - should fail
    result = flock(fd2, LOCK_SH | LOCK_NB);
    test_assert(result == -1, "Non-blocking shared lock fails (as expected)");

    // Release exclusive lock
    flock(fd1, LOCK_UN);

    // Now shared lock should succeed
    result = flock(fd2, LOCK_SH | LOCK_NB);
    test_assert(result == 0, "Non-blocking shared lock succeeds after release");

    flock(fd2, LOCK_UN);
    close(fd1);
    close(fd2);
}

void test_shared_blocks_exclusive()
{
    printf("\nTest 5: Shared Lock Blocks Exclusive (Non-blocking)\n");
    print_separator();

    int fd1 = open(TEST_FILE, O_RDWR);
    int fd2 = open(TEST_FILE, O_RDWR);

    test_assert(fd1 >= 0 && fd2 >= 0, "Both file descriptors opened");

    if (fd1 < 0 || fd2 < 0)
    {
        if (fd1 >= 0)
            close(fd1);
        if (fd2 >= 0)
            close(fd2);
        return;
    }

    // Acquire shared lock on fd1
    int result = flock(fd1, LOCK_SH);
    test_assert(result == 0, "Shared lock acquired on fd1");

    // Try non-blocking exclusive lock on fd2 - should fail
    result = flock(fd2, LOCK_EX | LOCK_NB);
    test_assert(result == -1, "Non-blocking exclusive lock fails (as expected)");

    // Release shared lock
    flock(fd1, LOCK_UN);

    // Now exclusive lock should succeed
    result = flock(fd2, LOCK_EX | LOCK_NB);
    test_assert(result == 0, "Non-blocking exclusive lock succeeds after release");

    flock(fd2, LOCK_UN);
    close(fd1);
    close(fd2);
}

void test_lock_upgrade()
{
    printf("\nTest 6: Lock Upgrade (Shared to Exclusive)\n");
    print_separator();

    int fd = open(TEST_FILE, O_RDWR);
    test_assert(fd >= 0, "File opened successfully");

    if (fd < 0)
        return;

    // Acquire shared lock
    int result = flock(fd, LOCK_SH);
    test_assert(result == 0, "Shared lock acquired");

    // Upgrade to exclusive lock
    result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Upgraded to exclusive lock");

    // Release
    flock(fd, LOCK_UN);
    close(fd);
}

void test_lock_downgrade()
{
    printf("\nTest 7: Lock Downgrade (Exclusive to Shared)\n");
    print_separator();

    int fd = open(TEST_FILE, O_RDWR);
    test_assert(fd >= 0, "File opened successfully");

    if (fd < 0)
        return;

    // Acquire exclusive lock
    int result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Exclusive lock acquired");

    // Downgrade to shared lock
    result = flock(fd, LOCK_SH);
    test_assert(result == 0, "Downgraded to shared lock");

    // Release
    flock(fd, LOCK_UN);
    close(fd);
}

void test_auto_release_on_close()
{
    printf("\nTest 8: Auto-release on Close\n");
    print_separator();

    int fd1 = open(TEST_FILE, O_RDWR);
    test_assert(fd1 >= 0, "File opened successfully");

    if (fd1 < 0)
        return;

    // Acquire exclusive lock
    int result = flock(fd1, LOCK_EX);
    test_assert(result == 0, "Exclusive lock acquired");

    // Close without explicit unlock
    close(fd1);
    test_assert(1, "File closed (lock should auto-release)");

    // Open again and try to lock - should succeed immediately
    int fd2 = open(TEST_FILE, O_RDWR);
    result = flock(fd2, LOCK_EX | LOCK_NB);
    test_assert(result == 0, "Lock available after close (auto-released)");

    flock(fd2, LOCK_UN);
    close(fd2);
}

void test_invalid_fd()
{
    printf("\nTest 9: Invalid File Descriptor\n");
    print_separator();

    // Try to lock invalid fd
    int result = flock(-1, LOCK_SH);
    test_assert(result == -1, "Invalid fd returns error");

    result = flock(999, LOCK_EX);
    test_assert(result == -1, "Non-existent fd returns error");
}

void test_write_with_exclusive()
{
    printf("\nTest 10: Write Operation with Exclusive Lock\n");
    print_separator();

    int fd = open(TEST_FILE, O_RDWR);
    test_assert(fd >= 0, "File opened successfully");

    if (fd < 0)
        return;

    // Acquire exclusive lock
    int result = flock(fd, LOCK_EX);
    test_assert(result == 0, "Exclusive lock acquired");

    // Write to file
    const char *data = "Hello, mCertiKOS!";
    int written = write(fd, (char *)data, 17);
    test_assert(written == 17, "Data written successfully with exclusive lock");

    // Release and close
    flock(fd, LOCK_UN);
    close(fd);
}

void test_read_with_shared()
{
    printf("\nTest 11: Read Operation with Shared Lock\n");
    print_separator();

    int fd = open(TEST_FILE, O_RDWR);
    test_assert(fd >= 0, "File opened successfully");

    if (fd < 0)
        return;

    // Acquire shared lock
    int result = flock(fd, LOCK_SH);
    test_assert(result == 0, "Shared lock acquired");

    // Read from file
    char buffer[32];
    int bytes_read = read(fd, buffer, 17);
    test_assert(bytes_read >= 0, "Data read successfully with shared lock");

    // Release and close
    flock(fd, LOCK_UN);
    close(fd);
}

void run_all_tests()
{
    printf("\n");
    print_separator();
    printf(" mCertiKOS FILE LOCKING TEST SUITE\n");
    print_separator();
    printf("\nStarting tests...\n");

    // Run all tests
    test_basic_shared_lock();
    test_basic_exclusive_lock();
    test_multiple_shared_locks();
    test_exclusive_blocks_shared();
    test_shared_blocks_exclusive();
    test_lock_upgrade();
    test_lock_downgrade();
    test_auto_release_on_close();
    test_invalid_fd();
    test_write_with_exclusive();
    test_read_with_shared();

    /* Multi-process tests (requires flockchild helper program, elf_id 7). */
    test_multiprocess_exclusive_blocks_child();
    test_multiprocess_shared_blocks_child_exclusive();
    test_multiprocess_child_succeeds_after_release();

    // Blocking tests (no LOCK_NB): child should sleep until release.
    test_blocking_exclusive_waits_for_shared_release();
    test_blocking_shared_waits_for_exclusive_release();

    // Print summary
    printf("\n");
    print_separator();
    printf(" TEST SUMMARY\n");
    print_separator();
    printf("Total Tests:  %d\n", test_count);
    printf("Passed:       %d\n", pass_count);
    printf("Failed:       %d\n", fail_count);
    print_separator();

    if (fail_count == 0)
    {
        printf("\n *** ALL TESTS PASSED! ***\n\n");
    }
    else
    {
        printf("\n *** SOME TESTS FAILED ***\n\n");
    }

    // Stop helper before deleting coordination files
    helper_stop();

    // Clean up test and coordination files
    unlink(TEST_FILE);
    unlink(CHILD_MODE_FILE);
    unlink(CHILD_RESULT_FILE);
}

int main(int argc, char **argv)
{
    printf("\nFile Locking Test Program\n");
    printf("Testing flock() system call implementation\n");

    run_all_tests();

    return 0;
}
