#ifndef _KERN_FLOCK_IMPORT_H_
#define _KERN_FLOCK_IMPORT_H_

#ifdef _KERN_

#include <lib/spinlock.h>
#include <lib/types.h>
#include <lib/debug.h>

// Note: If condition variables are available, import them here
// For now, we'll use busy-waiting with spinlocks

#endif /* _KERN_ */

#endif /* !_KERN_FLOCK_IMPORT_H_ */
