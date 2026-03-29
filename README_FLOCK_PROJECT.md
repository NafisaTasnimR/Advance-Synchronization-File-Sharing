# mCertiKOS Advanced Synchronization File Sharing (flock) - Simple Project README

Goal: understand what was implemented and what the 16 tests prove.

## 1) What this project adds

You added BSD-style file locking (`flock`) to mCertiKOS.

This gives 4 operations:
- `LOCK_SH`: shared lock (many readers allowed)
- `LOCK_EX`: exclusive lock (only one writer allowed)
- `LOCK_UN`: unlock
- `LOCK_NB`: non-blocking mode (return immediately if lock cannot be taken)

In short:
- Many readers can read together.
- Writer needs full ownership.
- Lock conflicts are handled safely.

## 2) End-to-end call path (very important)

When user code calls `flock(fd, op)`, the flow is:

1. User macro in `user/include/file.h`
2. User syscall wrapper in `user/include/syscall.h` (`sys_flock`)
3. Trap dispatch in `kern/trap/TDispatch/TDispatch.c` (`case SYS_flock`)
4. Syscall handler in `kern/fs/sysfile.c` (`sys_flock`)
5. File layer logic in `kern/fs/file.c` (`file_flock`)
6. Core lock engine in `kern/flock/flock.c` (`flock_acquire` / `flock_release`)
7. Lock state stored inside inode (`kern/fs/inode.h`: `struct inode { flock_t flock; }`)

This is the main architecture of your solution.

## 3) What was implemented in each necessary file

## A. Core lock engine

### `kern/flock/flock.h`
- Added lock states:
  - `FLOCK_INACTIVE`
  - `FLOCK_SHARED`
  - `FLOCK_EXCLUSIVE`
- Added `flock_t` structure with:
  - spinlock for protection
  - counters for shared holders, waiting shared, waiting exclusive
  - wait queue arrays (by pid)
- Added lock constants: `LOCK_SH`, `LOCK_EX`, `LOCK_UN`, `LOCK_NB`

### `kern/flock/flock.c`
- `flock_init`: initialize lock state and wait queue
- `flock_acquire`: shared/exclusive acquisition logic
- `flock_release`: release logic + wake blocked threads
- Uses `thread_sleep` / `thread_wakeup` for blocking behavior
- Includes writer-priority behavior:
  - new shared requests wait if a writer is waiting

### `kern/flock/export.h`
- Exposes API to other kernel modules:
  - `flock_init`
  - `flock_acquire`
  - `flock_release`

### `kern/flock/Makefile.inc` and `kern/Makefile.inc`
- Added build integration so `flock.c` compiles into kernel.

## B. File system integration

### `kern/fs/inode.h`
- Added `flock_t flock` into inode.
- Meaning: lock is per-file (per inode), not just per file descriptor.

### `kern/fs/inode.c`
- In `inode_get`, calls `flock_init(&ip->flock)` when inode cache entry is reused.
- Prevents stale lock state.

### `kern/fs/file.h`
- Added `holding_flock` in `struct file`.
- Tracks what this descriptor currently holds.

### `kern/fs/file.c`
- In `file_alloc`, initialize `holding_flock = 0`.
- In `file_close`, auto-release held lock before inode_put.
- Added `file_flock(struct file *f, int operation)`:
  - validates lockable type (`FD_INODE`)
  - handles unlock (`LOCK_UN`)
  - handles upgrade/downgrade by releasing old lock then acquiring new
  - calls core flock engine
  - updates per-fd ownership tracking

## C. Syscall plumbing

### `kern/lib/syscall.h`
- Added syscall number: `SYS_flock`.

### `kern/trap/TDispatch/TDispatch.c`
- Added dispatcher case for `SYS_flock`.

### `kern/fs/sysfile.c` and `kern/fs/sysfile.h`
- Added `sys_flock(tf_t *tf)`.
- Validates fd and file pointer.
- Calls `file_flock`.
- Returns:
  - success -> retval `0`
  - failure -> retval `-1` and errno set

### `user/include/syscall.h`
- Added user inline wrapper `sys_flock(int fd, int operation)`.

### `user/include/file.h`
- Added user lock flags (`LOCK_SH`, `LOCK_EX`, `LOCK_UN`, `LOCK_NB`).
- Added macro `flock(fd, op)` -> `sys_flock(fd, op)`.

## D. Test programs and build integration

### `user/flocktest/flocktest.c`
- Main test suite with 16 tests.
- Prints `[PASS]` / `[FAIL]` and final summary.

### `user/flockchild/flockchild.c`
- Persistent helper child process for multi-process tests.
- Reads mode commands from `/flockchild.mode`.
- Writes result codes to `/flockchild.res`.
- Uses nonce values to avoid reading stale results.

### `user/flocktest/Makefile.inc`, `user/flockchild/Makefile.inc`, `user/Makefile.inc`
- Ensures both test binaries are built and packed.

## 4) Important locking behavior to understand

1. Per-inode lock:
- Same file opened by different fds still shares one lock state.

2. Per-fd ownership tracking:
- Each fd knows what lock it holds (`holding_flock`).
- Makes close-time cleanup reliable.

3. Auto-release on close:
- If a process closes an fd that has lock, kernel releases lock automatically.

4. Upgrade/downgrade:
- SH -> EX and EX -> SH supported through `file_flock` logic.

5. Non-blocking mode:
- `LOCK_NB` returns immediately with failure if lock is not available.

6. Blocking mode:
- Without `LOCK_NB`, caller sleeps and wakes when lock may become available.

7. Writer priority:
- If writers are waiting, new readers are delayed.
- Helps prevent writer starvation.

## 5) The 16 tests (simple explanation)

Every test has 3 parts:
- What it does
- How it is implemented
- What you should learn

## Test 1: Basic Shared Lock
- What it does: takes and releases `LOCK_SH`.
- How: open file, `flock(fd, LOCK_SH)`, then `flock(fd, LOCK_UN)`.
- Learn: shared lock basic path works.

## Test 2: Basic Exclusive Lock
- What it does: takes and releases `LOCK_EX`.
- How: open file, `flock(fd, LOCK_EX)`, then unlock.
- Learn: exclusive lock basic path works.

## Test 3: Multiple Shared Locks (same process)
- What it does: two descriptors hold shared lock together.
- How: open same file twice (`fd1`, `fd2`), both call `LOCK_SH`.
- Learn: concurrent readers are allowed.

## Test 4: Exclusive Blocks Shared (non-blocking)
- What it does: checks conflict EX vs SH.
- How:
  - `fd1` gets `LOCK_EX`
  - `fd2` tries `LOCK_SH | LOCK_NB` and must fail
  - after unlock, same call must succeed
- Learn: exclusive lock blocks shared lock correctly.

## Test 5: Shared Blocks Exclusive (non-blocking)
- What it does: checks conflict SH vs EX.
- How:
  - `fd1` gets `LOCK_SH`
  - `fd2` tries `LOCK_EX | LOCK_NB` and must fail
  - after unlock, it succeeds
- Learn: reader blocks writer while reader holds lock.

## Test 6: Lock Upgrade (SH -> EX)
- What it does: upgrade in same fd.
- How: acquire shared, then call exclusive.
- Learn: upgrade path is implemented.

## Test 7: Lock Downgrade (EX -> SH)
- What it does: downgrade in same fd.
- How: acquire exclusive, then call shared.
- Learn: downgrade path is implemented.

## Test 8: Auto-release on Close
- What it does: verifies cleanup without explicit unlock.
- How:
  - fd1 takes exclusive
  - close(fd1) without `LOCK_UN`
  - new fd2 can lock immediately
- Learn: kernel releases lock on close.

## Test 9: Invalid FD
- What it does: bad fd handling.
- How: call flock on `-1` and `999`.
- Learn: invalid descriptor returns failure safely.

## Test 10: Write with Exclusive Lock
- What it does: write while holding exclusive lock.
- How: lock EX, write fixed string, verify bytes written.
- Learn: expected write workflow under EX lock works.

## Test 11: Read with Shared Lock
- What it does: read while holding shared lock.
- How: lock SH, perform read.
- Learn: expected read workflow under SH lock works.

## Test 12: Multi-process Exclusive Blocks Child (non-blocking)
- What it does: parent EX lock blocks child attempts.
- How:
  - parent holds EX
  - helper child tries SH|NB and EX|NB (`MODE_TRY_SH_EX_NB`)
  - both should fail
- Learn: lock conflicts are real across processes, not only inside one process.

## Test 13: Multi-process Shared Blocks Child Exclusive (non-blocking)
- What it does: parent SH should block child EX attempt.
- How:
  - parent holds SH
  - child tries EX|NB (`MODE_TRY_EX_NB`) and should fail
- Learn: reader lock held by one process blocks writer from another process.

## Test 14: Multi-process Child Succeeds After Release (non-blocking)
- What it does: child can acquire after parent unlocks.
- How:
  - parent takes EX then unlocks
  - child tries EX|NB (`MODE_ACQ_EX_NB`) and should succeed
- Learn: lock release is visible system-wide.

## Test 15: Blocking Exclusive Waits For Shared Release
- What it does: true blocking behavior for EX request.
- How:
  - parent takes SH
  - child requests blocking EX (`MODE_ACQ_EX_BLOCK`)
  - parent checks child has not finished yet (still blocked)
  - parent unlocks, child then succeeds
- Learn: blocking wait/sleep path works correctly for exclusive waiter.

## Test 16: Blocking Shared Waits For Exclusive Release
- What it does: true blocking behavior for SH request.
- How:
  - parent takes EX
  - child requests blocking SH (`MODE_ACQ_SH_BLOCK`)
  - child should remain blocked until parent unlocks
  - after unlock, child succeeds
- Learn: blocking wait/sleep path works correctly for shared waiter.

## 6) How the multi-process tests are implemented

These tests are clever and important.

Parent (`flocktest`) and child (`flockchild`) communicate using files:
- command file: `/flockchild.mode`
- result file: `/flockchild.res`

Protocol:
- Parent writes command: `M<mode>:<nonce>`
- Child performs requested lock operation
- Child writes result: `R<code>:<nonce>`

Why nonce is used:
- avoids mixing old result with new request
- parent knows exactly which response belongs to current test

Result code meaning (in this implementation):
- `0` means child observed expected behavior
- non-zero means mismatch/error

## 7) What you should interpret from PASS/FAIL style

If a test passes, interpret it like this:
- The specific lock rule for that scenario is correct.

If all 16 pass, interpret it like this:
- basic lock operations work
- conflict detection works
- non-blocking mode works
- blocking sleep/wakeup works
- multi-process correctness works
- auto-cleanup on close works

This gives strong confidence that your advanced synchronization file sharing logic is correct for expected use cases.

## 8) Current note about existing test README

`user/flocktest/README.md` still says 11 tests.
But actual code in `user/flocktest/flocktest.c` runs 16 tests.
Use code as source of truth.

## 9) How to run quickly

1. `make`
2. `make qemu`
3. In shell: `flocktest`

You should see per-assert `[PASS]/[FAIL]` and a final summary.

If needed, I can also update `user/flocktest/README.md` so it fully matches the new 16-test implementation.