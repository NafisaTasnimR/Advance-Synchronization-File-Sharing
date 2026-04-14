# FLOCK Project Overview

## Introduction
The FLOCK project extends the mCertiKOS educational operating system with BSD-style file locking support using `flock` semantics. The implementation adds safe synchronization for concurrent file access so that multiple processes can coordinate read and write operations without corrupting shared data.

## Project Goal
The main goal is to implement and validate kernel-level file locking with correct behavior for shared and exclusive access patterns:

- Shared lock (`LOCK_SH`): allows multiple readers.
- Exclusive lock (`LOCK_EX`): allows a single writer.
- Unlock (`LOCK_UN`): releases the current lock.
- Non-blocking flag (`LOCK_NB`): returns immediately if lock cannot be acquired.

## High-Level Architecture
The implementation follows the full user-to-kernel syscall path:

1. User program calls `flock(fd, operation)`.
2. User syscall wrapper forwards the request to kernel mode.
3. Trap/syscall dispatcher routes the call to `sys_flock`.
4. File-system layer validates file descriptor and delegates to file lock manager.
5. Core flock engine applies lock-state rules and synchronization.
6. Lock state is maintained per inode so all descriptors of the same file share one lock state.

## Core Design Ideas

- Per-inode locking model for consistent file-level coordination.
- Per-file-descriptor tracking for ownership and cleanup.
- Automatic lock release when a file descriptor is closed.
- Upgrade/downgrade handling between shared and exclusive modes.
- Writer-priority behavior to reduce writer starvation.
- Blocking and non-blocking lock acquisition modes.

## Testing and Validation
The project includes user-space test programs to verify correctness across:

- Basic shared and exclusive lock acquisition/release.
- Conflict handling between readers and writers.
- Lock upgrade and downgrade scenarios.
- Invalid file descriptor handling.
- Auto-release behavior on close.
- Multi-process contention and synchronization behavior.

## Project Outcome
This project demonstrates how operating system synchronization concepts can be integrated into a real kernel code path, from user APIs to low-level lock state management, while preserving correctness under concurrent access.

## Team Members
- Sanjana Afreen [220042106]
- Nishat Tasneem [220042110]
- Nafisa Tasnim [220042114]
- Nishat Tasnim [220042124]
- Mrittika Jahan [220042150]
