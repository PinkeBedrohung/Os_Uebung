#include "PidWaits.h"
#include "ArchThreads.h"

PidWaits::PidWaits(size_t pid) : list_lock_("PidWaits::list_lock_"), pid_(pid), cond_lock_("PidWaits::cond_lock_"), pid_ready_cond_(&cond_lock_,"pid_ready_cond_")
{
}

PidWaits::~PidWaits()
{
}

void PidWaits::addTid(size_t tid)
{
    MutexLock lock(list_lock_);
    tids_.push_back(tid);
}

void PidWaits::removeTid(size_t tid)
{
    assert(list_lock_.isHeldBy(currentThread));
    for (auto itr = tids_.begin(); itr != tids_.end(); itr++)
    {
        if(tid == *itr)
        {
            tids_.erase(itr);
            break;
        }
    }
}

void PidWaits::signalReady()
{
    MutexLock lock(list_lock_);
    cond_lock_.acquire();
    pid_ready_cond_.broadcast();
    cond_lock_.release();
}

void PidWaits::waitUntilReady(size_t tid)
{
    addTid(tid);
    cond_lock_.acquire();
    pid_ready_cond_.waitAndRelease();
}

size_t PidWaits::getPid()
{
    return pid_;
}

size_t PidWaits::getNumTids()
{
    assert(list_lock_.isHeldBy(currentThread));
    return tids_.size();
}