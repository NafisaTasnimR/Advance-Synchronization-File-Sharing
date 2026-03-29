#ifndef _KERN_FLOCK_FLOCK_H_
#define _KERN_FLOCK_FLOCK_H_

#ifdef _KERN_

#include <lib/types.h>
#include <lib/spinlock.h>
#include <lib/x86.h>

/**
 * File lock states
 */
enum flock_state
{
    FLOCK_INACTIVE = 0, // No locks held
    FLOCK_SHARED,       // Shared (read) locks held
    FLOCK_EXCLUSIVE     // Exclusive (write) lock held
};

/**
 * File lock structure - embedded in inode
 * This makes lock state per-inode (shared across all fds to same file)
 */
typedef struct flock_t
{
    spinlock_t lock;        // Protects the flock structure
    enum flock_state state; // Current lock state
    int shared_holders;     // Number of shared lock holders
    int waiting_shared;     // Number of waiting shared lock requests
    int waiting_exclusive;  // Number of waiting exclusive lock requests

    // Wait queue (per-inode): tracks processes waiting for this lock.
    // Blocking callers sleep on the channel equal to this flock_t pointer.
    struct
    {
        unsigned int pids[NUM_IDS];
        unsigned int len;
        uint8_t inq[NUM_IDS];
        uint8_t req[NUM_IDS]; // LOCK_SH or LOCK_EX, indexed by pid
    } waitq;
} flock_t;

/**
 * Lock operation flags (matching POSIX flock)
 */
#define LOCK_SH 1 // Shared lock
#define LOCK_EX 2 // Exclusive lock
#define LOCK_UN 8 // Unlock
#define LOCK_NB 4 // Non-blocking mode

#endif /* _KERN_ */

#endif /* !_KERN_FLOCK_FLOCK_H_ */
