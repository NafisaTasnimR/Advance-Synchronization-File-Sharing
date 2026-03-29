#include "import.h"
#include "flock.h"

#include <thread/PThread/export.h>
#include <thread/PCurID/export.h>

static void waitq_add(flock_t *fl, unsigned int pid, int req)
{
    if (pid >= NUM_IDS)
    {
        return;
    }
    if (fl->waitq.inq[pid])
    {
        fl->waitq.req[pid] = (uint8_t)req;
        return;
    }
    if (fl->waitq.len < NUM_IDS)
    {
        fl->waitq.pids[fl->waitq.len++] = pid;
        fl->waitq.inq[pid] = 1;
        fl->waitq.req[pid] = (uint8_t)req;
    }
}

static void waitq_remove(flock_t *fl, unsigned int pid)
{
    unsigned int i;

    if (pid >= NUM_IDS || !fl->waitq.inq[pid])
    {
        return;
    }

    for (i = 0; i < fl->waitq.len; i++)
    {
        if (fl->waitq.pids[i] == pid)
        {
            for (; i + 1 < fl->waitq.len; i++)
            {
                fl->waitq.pids[i] = fl->waitq.pids[i + 1];
            }
            fl->waitq.len--;
            break;
        }
    }

    fl->waitq.inq[pid] = 0;
    fl->waitq.req[pid] = 0;
}

/**
 * Initialize a file lock structure
 */
void flock_init(flock_t *fl)
{
    spinlock_init(&fl->lock);
    fl->state = FLOCK_INACTIVE;
    fl->shared_holders = 0;
    fl->waiting_shared = 0;
    fl->waiting_exclusive = 0;

    fl->waitq.len = 0;
    for (unsigned int i = 0; i < NUM_IDS; i++)
    {
        fl->waitq.inq[i] = 0;
        fl->waitq.req[i] = 0;
    }
}

/**
 * Try to acquire a shared lock
 * Returns 0 on success, -1 if would block
 */
static int try_acquire_shared(flock_t *fl, int nonblocking)
{
    unsigned int pid = get_curid();

    // Can acquire shared lock if no exclusive lock is held
    // and no exclusive waiters (to prevent writer starvation)
    if (fl->state == FLOCK_EXCLUSIVE || fl->waiting_exclusive > 0)
    {
        if (nonblocking)
        {
            return -1; // Would block
        }

        waitq_add(fl, pid, LOCK_SH);
        fl->waiting_shared++;
        while (fl->state == FLOCK_EXCLUSIVE || fl->waiting_exclusive > 0)
        {
            thread_sleep(fl, &fl->lock);
        }
        fl->waiting_shared--;
        waitq_remove(fl, pid);
    }

    fl->state = FLOCK_SHARED;
    fl->shared_holders++;
    return 0;
}

/**
 * Try to acquire an exclusive lock
 * Returns 0 on success, -1 if would block
 */
static int try_acquire_exclusive(flock_t *fl, int nonblocking)
{
    unsigned int pid = get_curid();

    // Can acquire exclusive lock only if no locks are held
    if (fl->state != FLOCK_INACTIVE)
    {
        if (nonblocking)
        {
            return -1; // Would block
        }

        waitq_add(fl, pid, LOCK_EX);
        fl->waiting_exclusive++;
        while (fl->state != FLOCK_INACTIVE)
        {
            thread_sleep(fl, &fl->lock);
        }
        fl->waiting_exclusive--;
        waitq_remove(fl, pid);
    }

    fl->state = FLOCK_EXCLUSIVE;
    return 0;
}

/**
 * Acquire a file lock
 */
int flock_acquire(flock_t *fl, int operation)
{
    int result = 0;
    int nonblocking = (operation & LOCK_NB) != 0;
    int lock_type = operation & ~LOCK_NB;

    spinlock_acquire(&fl->lock);

    switch (lock_type)
    {
    case LOCK_SH:
        result = try_acquire_shared(fl, nonblocking);
        break;

    case LOCK_EX:
        result = try_acquire_exclusive(fl, nonblocking);
        break;

    case LOCK_UN:
        // Unlock is handled by flock_release
        result = 0;
        break;

    default:
        result = -1; // Invalid operation
        break;
    }

    spinlock_release(&fl->lock);
    return result;
}

/**
 * Release a file lock
 */
int flock_release(flock_t *fl, int lock_type)
{
    int need_wakeup = 0;

    spinlock_acquire(&fl->lock);

    switch (lock_type)
    {
    case LOCK_SH:
        if (fl->state == FLOCK_SHARED && fl->shared_holders > 0)
        {
            fl->shared_holders--;
            if (fl->shared_holders == 0)
            {
                fl->state = FLOCK_INACTIVE;
                need_wakeup = 1;
            }
        }
        break;

    case LOCK_EX:
        if (fl->state == FLOCK_EXCLUSIVE)
        {
            fl->state = FLOCK_INACTIVE;
            need_wakeup = 1;
        }
        break;

    default:
        spinlock_release(&fl->lock);
        return -1;
    }

    if (need_wakeup)
    {
        thread_wakeup(fl);
    }

    spinlock_release(&fl->lock);
    return 0;
}
