# Flock Project Split for 5 People

This split is designed for:
- Group A: 2 people
- Group B: 3 people

Goal:
- Each group studies a different layer.
- Together, both groups cover the full project end-to-end.

## First Common Reading (All 5 people)

Everyone should read these first:
- [FLOCK_IMPLEMENTATION.md](FLOCK_IMPLEMENTATION.md)
- [README_FLOCK_PROJECT.md](README_FLOCK_PROJECT.md)

After that, move to your group files below.

## Group A (2 people) - Kernel Lock Core + FS Lock Integration

Main responsibility:
Understand how lock state is stored and enforced inside kernel file/inode layers.

### A1 (Person 1): Core lock engine

Study these files:
- [kern/flock/flock.h](kern/flock/flock.h)
- [kern/flock/flock.c](kern/flock/flock.c)
- [kern/flock/export.h](kern/flock/export.h)
- [kern/flock/import.h](kern/flock/import.h)
- [kern/flock/Makefile.inc](kern/flock/Makefile.inc)

What A1 should explain to team:
- lock states: inactive/shared/exclusive
- blocking vs non-blocking behavior
- wait queue and sleep/wakeup logic
- writer-priority idea

### A2 (Person 2): File system integration

Study these files:
- [kern/fs/inode.h](kern/fs/inode.h)
- [kern/fs/inode.c](kern/fs/inode.c)
- [kern/fs/file.h](kern/fs/file.h)
- [kern/fs/file.c](kern/fs/file.c)

What A2 should explain to team:
- why lock is inside inode (per-file/per-inode)
- why holding_flock is inside file descriptor object
- file close auto-release behavior
- upgrade/downgrade flow in file_flock

Group A output:
- One diagram: inode lock state + fd ownership relation
- One short note: how deadlock/starvation is reduced in current design

## Group B (3 people) - Syscall Path + User Test System

Main responsibility:
Understand user-to-kernel syscall path and how 16 tests validate behavior.

### B1 (Person 1): Syscall plumbing (user -> kernel)

Study these files:
- [kern/lib/syscall.h](kern/lib/syscall.h)
- [kern/trap/TDispatch/TDispatch.c](kern/trap/TDispatch/TDispatch.c)
- [kern/fs/sysfile.h](kern/fs/sysfile.h)
- [kern/fs/sysfile.c](kern/fs/sysfile.c)
- [user/include/syscall.h](user/include/syscall.h)
- [user/include/file.h](user/include/file.h)

What B1 should explain to team:
- how SYS_flock is registered and dispatched
- how arguments move from user call to kernel handler
- return value and errno mapping

### B2 (Person 2): Main test suite (single-process + summaries)

Study these files:
- [user/flocktest/flocktest.c](user/flocktest/flocktest.c)
- [user/flocktest/README.md](user/flocktest/README.md)

Focus on these tests in flocktest.c:
- tests 1 to 11 (basic locking, upgrade/downgrade, auto-release, invalid fd, read/write)

What B2 should explain to team:
- how each test checks one lock rule
- how PASS/FAIL counters and assertions are organized
- why non-blocking tests are safe for quick validation

### B3 (Person 3): Multi-process helper + build/run integration

Study these files:
- [user/flockchild/flockchild.c](user/flockchild/flockchild.c)
- [user/flockchild/Makefile.inc](user/flockchild/Makefile.inc)
- [user/flocktest/Makefile.inc](user/flocktest/Makefile.inc)
- [user/Makefile.inc](user/Makefile.inc)
- [user/config.mk](user/config.mk)
- [user/idle/idle.c](user/idle/idle.c)
- [user/shell/commands.c](user/shell/commands.c)
- [user/shell/shell.c](user/shell/shell.c)

Focus on these tests in flocktest.c:
- tests 12 to 16 (multi-process + blocking behavior)

What B3 should explain to team:
- parent-child coordination via mode/result files
- nonce usage and why it avoids stale responses
- why helper process is needed for realistic lock contention
- how flocktest/flockchild are built and launched

Group B output:
- One flowchart: user flock call -> syscall -> kernel -> return
- One flowchart: flocktest parent-child interaction for tests 12-16

## Final Cross-Group Sharing Plan (Simple)

1. Group A presents kernel internals in 15 minutes.
2. Group B presents syscall + test methodology in 20 minutes.
3. Both groups do 10-minute combined Q/A on:
   - lock ownership model
   - blocking/non-blocking semantics
   - what each of 16 tests proves

## Quick Balance Check

- Group A has fewer files but deeper kernel complexity.
- Group B has more files but easier user-space tracing and test interpretation.
- This keeps effort balanced for a 2-person vs 3-person split.
