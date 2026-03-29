# FLOCK Project Report

## Course Information
- Course: CSE 4502 Operating System Lab
- Project: File Locking (FLOCK) in mCertiKOS
- Date: March 29, 2026

## 1. Executive Summary
This project implements BSD-style file locking in the mCertiKOS operating system. The main purpose is to provide safe synchronization when multiple processes access the same file concurrently. The implementation introduces shared and exclusive lock semantics, supports blocking and non-blocking behavior, and integrates lock management from user-space API down to kernel-level inode state.

## 2. Project Objectives
The key objectives of the project were:

1. Implement file locking semantics compatible with flock behavior.
2. Support read-sharing through shared locks.
3. Ensure exclusive write access through exclusive locks.
4. Provide non-blocking lock acquisition mode.
5. Integrate locking into the kernel file-system path.
6. Validate functionality with comprehensive user-space tests.

## 3. Locking Model and Semantics
The project supports the following operations:

- LOCK_SH: Acquire a shared lock.
- LOCK_EX: Acquire an exclusive lock.
- LOCK_UN: Release a held lock.
- LOCK_NB: Non-blocking modifier for immediate failure on contention.

### 3.1 Shared Lock
A shared lock allows multiple holders at the same time. This is suitable for concurrent readers.

### 3.2 Exclusive Lock
An exclusive lock allows only one holder and blocks all other shared or exclusive requests.

### 3.3 Unlock
Unlock releases the lock currently associated with the requesting file descriptor context.

### 3.4 Non-Blocking Mode
When LOCK_NB is used, lock acquisition does not wait. If the lock cannot be granted immediately, the operation returns failure.

## 4. System Architecture
The design follows a layered syscall pipeline:

1. User application calls flock(fd, operation).
2. User syscall wrapper forwards request to kernel trap.
3. Trap dispatcher routes to SYS_flock handler.
4. System call layer validates file descriptor and request.
5. File layer applies descriptor-level lock ownership rules.
6. Core flock engine enforces lock state and contention policy.
7. Inode stores lock state so all descriptors of the same file share one synchronization object.

## 5. Kernel-Level Design Highlights

### 5.1 Per-Inode Lock State
Lock state is attached to inode objects, ensuring all opens of the same file coordinate through one lock state.

### 5.2 Per-Descriptor Ownership Tracking
Each file descriptor tracks the lock it currently holds. This enables consistent unlock behavior, upgrade or downgrade handling, and clean resource release on close.

### 5.3 Automatic Cleanup
If a locked file descriptor is closed, the associated lock is automatically released to avoid lock leaks.

### 5.4 Upgrade and Downgrade Support
The implementation supports transitions between shared and exclusive ownership logic through controlled release and re-acquire paths.

### 5.5 Writer Priority Strategy
When exclusive waiters exist, new shared lock requests may be delayed to reduce writer starvation.

## 6. Implementation Scope
The implementation spans several kernel and user-space components:

- Core flock module for state and lock operations.
- File-system inode and file structures for lock embedding and ownership tracking.
- Syscall number registration and trap dispatch wiring.
- System call handler and file lock bridge function.
- User-space headers for flock flags and wrapper interface.
- Dedicated test binaries for single-process and multi-process validation.

## 7. Testing and Validation
The test suite validates both functional correctness and concurrency behavior, including:

- Basic shared and exclusive lock acquisition and release.
- Conflict behavior between readers and writers.
- Upgrade and downgrade correctness.
- Non-blocking lock outcomes under contention.
- Automatic unlock on close.
- Invalid file descriptor handling.
- Multi-process contention and release visibility.
- Blocking behavior for lock wait scenarios.

A total of 16 tests were used to verify the behavior across these cases.

## 8. Challenges and Solutions

### 8.1 Challenge: Consistent Coordination Across Multiple File Descriptors
Solution: Move lock state to inode level and track descriptor ownership separately.

### 8.2 Challenge: Avoiding Lock Leaks
Solution: Add auto-release logic during file descriptor close path.

### 8.3 Challenge: Fairness Under Contention
Solution: Introduce writer-priority behavior so writers are not indefinitely starved by incoming readers.

## 9. Learning Outcomes
Through this project, the team gained practical understanding of:

- Kernel synchronization primitives and lock state machines.
- Syscall design and user-kernel interface flow.
- File-system integration techniques in a teaching OS.
- Testing concurrent behavior in single and multi-process settings.

## 10. Conclusion
The FLOCK project successfully integrates robust file locking into mCertiKOS with clear lock semantics, syscall-level connectivity, and extensive validation coverage. The final system demonstrates correct behavior for both routine and contended access patterns, making concurrent file access safer and more predictable.

## 11. Team Members
- Sanjana Afreen [220042106]
- Nishat Tasneem [220042110]
- Nafisa Tasnim [220042114]
- Nishat Tasnim [220042124]
- Mrittika Jahan [220042150]
