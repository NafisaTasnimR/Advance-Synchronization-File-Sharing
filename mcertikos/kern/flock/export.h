#ifndef _KERN_FLOCK_EXPORT_H_
#define _KERN_FLOCK_EXPORT_H_

#ifdef _KERN_

#include "flock.h"

/**
 * Initialize a file lock structure
 * Should be called when an inode is allocated or enters cache
 */
void flock_init(flock_t *fl);

/**
 * Acquire a file lock
 * @param fl - the file lock structure
 * @param operation - LOCK_SH, LOCK_EX, or LOCK_UN combined with optional LOCK_NB
 * @return 0 on success, -1 on error (e.g., would block with LOCK_NB)
 */
int flock_acquire(flock_t *fl, int operation);

/**
 * Release a file lock
 * @param fl - the file lock structure
 * @param lock_type - LOCK_SH or LOCK_EX (what type of lock to release)
 * @return 0 on success, -1 on error
 */
int flock_release(flock_t *fl, int lock_type);

#endif /* _KERN_ */

#endif /* !_KERN_FLOCK_EXPORT_H_ */
