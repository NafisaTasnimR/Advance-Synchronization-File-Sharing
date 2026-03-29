# mCertiKOS File Locking Test

This directory contains the user-space test program for the file locking (flock) system implementation.

## Overview

The `flocktest` program is a comprehensive test suite that validates the flock() system call implementation in mCertiKOS. It runs **completely automatically with no user input required**.

## Building

The test program is built as part of the normal mCertiKOS build process:

```bash
cd /path/to/mcertikos
make
```

This will compile `flocktest` and include it in the kernel image.

## Running in QEMU

After building, run mCertiKOS in QEMU:

```bash
make qemu
```

In the mCertiKOS shell, run the test program:

```bash
flocktest
```

The test will run automatically and display results with [PASS] or [FAIL] indicators.

## Test Coverage

### Tests Included (11 total):

1. **Basic Shared Lock**
   - Acquire and release a shared lock
   - Verify basic functionality

2. **Basic Exclusive Lock**
   - Acquire and release an exclusive lock
   - Verify exclusive lock behavior

3. **Multiple Shared Locks**
   - Open same file multiple times
   - Acquire shared locks on all descriptors
   - Verify multiple readers can coexist

4. **Exclusive Blocks Shared**
   - Hold exclusive lock
   - Try non-blocking shared lock (should fail)
   - Release and verify shared lock succeeds

5. **Shared Blocks Exclusive**
   - Hold shared lock
   - Try non-blocking exclusive lock (should fail)
   - Release and verify exclusive lock succeeds

6. **Lock Upgrade**
   - Acquire shared lock
   - Upgrade to exclusive lock
   - Verify upgrade works correctly

7. **Lock Downgrade**
   - Acquire exclusive lock
   - Downgrade to shared lock
   - Verify downgrade works correctly

8. **Auto-release on Close**
   - Acquire lock
   - Close file descriptor without explicit unlock
   - Verify lock is automatically released

9. **Invalid File Descriptor**
   - Try to lock invalid fd (-1, 999)
   - Verify error handling

10. **Write with Exclusive Lock**
    - Acquire exclusive lock
    - Perform write operation
    - Verify write succeeds

11. **Read with Shared Lock**
    - Acquire shared lock
    - Perform read operation
    - Verify read succeeds

## Expected Output

```
========================================
 mCertiKOS FILE LOCKING TEST SUITE
========================================

Starting tests...

Test 1: Basic Shared Lock
========================================
  [PASS] File opened successfully
  [PASS] Shared lock acquired
  [PASS] Lock released

Test 2: Basic Exclusive Lock
========================================
  [PASS] File opened successfully
  [PASS] Exclusive lock acquired
  [PASS] Lock released

... (more tests) ...

========================================
 TEST SUMMARY
========================================
Total Tests:  (number)
Passed:       (number)
Failed:       0
========================================

 *** ALL TESTS PASSED! ***
```

## No Input Required

**Important:** This test program requires **NO USER INPUT**. It runs completely automatically from start to finish. Just execute `flocktest` and watch the results.

## Files

- **flocktest.c** - Main test program source code
- **Makefile.inc** - Build configuration for integration with mCertiKOS build system
- **README.md** - This file

## Integration Points

The test program uses:
- `flock()` - System call wrapper (defined in user/include/syscall.h)
- `open()`, `close()`, `read()`, `write()` - File operations
- `printf()` - Output formatting
- Standard mCertiKOS user-space libraries

## Troubleshooting

### Build Errors

**Error: undefined reference to `sys_flock`**
- Make sure user/include/syscall.h has the sys_flock() wrapper function
- Verify user/include/file.h defines LOCK_SH, LOCK_EX, LOCK_UN, LOCK_NB constants

**Error: SYS_flock undeclared**
- Check that kern/lib/syscall.h has the SYS_flock enum value
- Ensure the kernel-side implementation is complete

### Runtime Errors

**Test hangs or freezes**
- Check if the flock implementation has deadlock issues
- Verify spinlock release in wait loops
- Look for busy-wait loops without proper yielding

**All tests fail**
- Verify the system call dispatcher routes SYS_flock correctly
- Check that sys_flock() in kern/fs/sysfile.c is implemented
- Verify file_flock() in kern/fs/file.c is working

**Specific test fails**
- Check the test output to see which assertion failed
- Review the corresponding flock functionality
- Add debug prints to kernel code if needed

## Adding More Tests

To add additional tests:

1. Create a new test function:
```c
void test_my_feature() {
    printf("\nTest X: My Feature\n");
    print_separator();
    
    // Test code here
    test_assert(condition, "Description");
    
    // Cleanup
}
```

2. Add to `run_all_tests()`:
```c
void run_all_tests() {
    // ... existing tests ...
    test_my_feature();
    // ...
}
```

## Notes

- Test file `/testfile` is created and cleaned up automatically
- Each test is independent and doesn't affect others
- Non-blocking tests avoid hanging the system
- Tests verify both success and failure cases
- Auto-release test verifies kernel cleanup

## Multi-process Testing

For testing across multiple processes (future enhancement):

```c
pid_t pid = sys_spawn(program_id, quota);
if (pid > 0) {
    // Parent process
    // Test concurrent access
} else {
    // Child process
    // Acquire locks
}
```

This would require additional coordination and is not included in the current test suite.
