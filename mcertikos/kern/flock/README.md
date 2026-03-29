# File Locking Implementation

This directory contains the kernel-side implementation of BSD-style file locking (flock) for mCertiKOS.

## Files

- **flock.h** - Core data structures and constants
- **flock.c** - Lock state machine implementation
- **export.h** - Public API for other kernel modules
- **import.h** - Dependencies
- **Makefile.inc** - Build configuration

## Testing

The test program is located in the **user-space** directory, not here:

```
user/flocktest/
```

To run the tests:

1. Build mCertiKOS:
   ```bash
   make
   ```

2. Run in QEMU:
   ```bash
   make qemu
   ```

3. In the mCertiKOS shell, execute:
   ```bash
   flocktest
   ```

The test runs automatically with no input required and displays [PASS]/[FAIL] results.

See `user/flocktest/README.md` for detailed test documentation.

## Implementation Details

### Lock States
- **FLOCK_INACTIVE** - No locks held
- **FLOCK_SHARED** - One or more shared locks held
- **FLOCK_EXCLUSIVE** - Exclusive lock held

### Operations
- **flock_init()** - Initialize lock structure
- **flock_acquire()** - Acquire shared or exclusive lock
- **flock_release()** - Release held lock

### Features
- Multiple shared readers
- Single exclusive writer
- Writer priority (prevents writer starvation)
- Non-blocking mode
- Auto-release on file descriptor close
- Lock upgrade/downgrade support

### Integration
- Per-inode lock state (struct inode.flock)
- Per-fd lock tracking (struct file.holding_flock)
- System call interface (SYS_flock)

For complete implementation details, see:
- `FLOCK_IMPLEMENTATION.md` (if exists in root directory)
- Source code comments in flock.c and flock.h
