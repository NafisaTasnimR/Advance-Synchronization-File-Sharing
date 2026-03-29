/*
 * Persistent helper program for multi-process flock tests.
 *
 * Parent writes desired action into /flockchild.mode as:   M<mode>:<nonce>\n
 * Child writes result into /flockchild.res as:            R<code>:<nonce>\n
 * Child loops until it receives MODE_EXIT.
 */

#include <proc.h>
#include <stdio.h>
#include <syscall.h>
#include <types.h>
#include <file.h>

#define TEST_FILE   "/testfile"
#define MODE_FILE   "/flockchild.mode"
#define RESULT_FILE "/flockchild.res"

enum {
    MODE_TRY_SH_NB = 1,
    MODE_TRY_EX_NB = 2,
    MODE_ACQ_SH_NB = 3,
    MODE_ACQ_EX_NB = 4,
    MODE_TRY_SH_EX_NB = 5,
    MODE_ACQ_SH_BLOCK = 6,
    MODE_ACQ_EX_BLOCK = 7,
    MODE_EXIT = 9,
};

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

static int read_mode_and_nonce(int *out_mode, int *out_nonce)
{
    int fd = open(MODE_FILE, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char buf[16];
    int n = read(fd, buf, sizeof(buf));
    close(fd);
    /* Expect fixed-width 'M<mode>:<nonce8>\n' (12 bytes). */
    if (n < 12) {
        return -1;
    }
    if (buf[0] != 'M' || buf[1] < '0' || buf[1] > '9') {
        return -1;
    }
    int mode = buf[1] - '0';
    int nonce = parse_nonce(buf, 12, 2);
    if (nonce < 0) {
        return -1;
    }
    *out_mode = mode;
    *out_nonce = nonce;
    return 0;
}

static void write_result_with_nonce(int code, int nonce)
{
    int fd = open(RESULT_FILE, O_CREATE | O_RDWR);
    if (fd < 0) {
        return;
    }

    /* Write fixed-width 'R<code>:<nonce8>\n' (12 bytes). */
    if (nonce < 0) {
        nonce = 0;
    }
    char buf[12];
    buf[0] = 'R';
    buf[1] = (char)('0' + (code % 10));
    buf[2] = ':';
    int n = nonce;
    for (int p = 0; p < 8; p++) {
        buf[10 - p] = (char)('0' + (n % 10));
        n /= 10;
    }
    buf[11] = '\n';
    write(fd, buf, 12);
    close(fd);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int last_nonce = -1;
    for (;;) {
        int mode, nonce;
        if (read_mode_and_nonce(&mode, &nonce) < 0) {
            for (int i = 0; i < 5; i++) {
                yield();
            }
            continue;
        }
        if (nonce == last_nonce) {
            for (int i = 0; i < 5; i++) {
                yield();
            }
            continue;
        }
        last_nonce = nonce;

        if (mode == MODE_EXIT) {
            write_result_with_nonce(0, nonce);
            return 0;
        }

        int fd = open(TEST_FILE, O_CREATE | O_RDWR);
        if (fd < 0) {
            write_result_with_nonce(8, nonce);
            continue;
        }

        int code = 1;

        if (mode == MODE_TRY_SH_EX_NB) {
            int r1 = flock(fd, LOCK_SH | LOCK_NB);
            int r2 = flock(fd, LOCK_EX | LOCK_NB);
            code = (r1 == -1 && r2 == -1) ? 0 : 1;
            close(fd);
            write_result_with_nonce(code, nonce);
            continue;
        }

        int want = 0;
        int expect_ok = 0;
        switch (mode) {
        case MODE_TRY_SH_NB:
            want = LOCK_SH | LOCK_NB;
            expect_ok = 0;
            break;
        case MODE_TRY_EX_NB:
            want = LOCK_EX | LOCK_NB;
            expect_ok = 0;
            break;
        case MODE_ACQ_SH_NB:
            want = LOCK_SH | LOCK_NB;
            expect_ok = 1;
            break;
        case MODE_ACQ_EX_NB:
            want = LOCK_EX | LOCK_NB;
            expect_ok = 1;
            break;
        case MODE_ACQ_SH_BLOCK:
            want = LOCK_SH;
            expect_ok = 1;
            break;
        case MODE_ACQ_EX_BLOCK:
            want = LOCK_EX;
            expect_ok = 1;
            break;
        default:
            close(fd);
            write_result_with_nonce(7, nonce);
            continue;
        }

        int r = flock(fd, want);
        code = (expect_ok ? (r == 0) : (r == -1)) ? 0 : 1;
        if (r == 0) {
            flock(fd, LOCK_UN);
        }
        close(fd);
        write_result_with_nonce(code, nonce);
    }
}
