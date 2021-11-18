#include "PidWaits.h"
#include "ArchThreads.h"

PidWaits::PidWaits(size_t pid) : list_lock_("PidWaits::list_lock_"), pid_to_wait_for_(pid), cond_lock_("PidWaits::cond_lock_"), pid_ready_cond_(&cond_lock_,"pid_ready_cond_")
{
}

PidWaits::~PidWaits()
{
    debug(WAIT_PID, "Deleted PidWaits PID: %ld\n", pid_to_wait_for_);
}

void PidWaits::addPtid(size_t pid, size_t tid)
{
    MutexLock lock(list_lock_);
    ptid elem;
    elem.pid_ = pid;
    elem.tid_ = tid;
    ptids_.push_back(elem);
}

void PidWaits::removePtid(size_t pid, size_t tid)
{
    assert(list_lock_.isHeldBy(currentThread));
    for (auto itr = ptids_.begin(); itr != ptids_.end(); itr++)
    {
        if(tid == (*itr).tid_ && pid == (*itr).pid_)
        {
            ptids_.erase(itr);
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

void PidWaits::waitUntilReady(size_t pid, size_t tid)
{
    addPtid(pid, tid);
    cond_lock_.acquire();
    pid_ready_cond_.waitAndRelease();
}

size_t PidWaits::getPid()
{
    return pid_to_wait_for_;
}

size_t PidWaits::getNumPtids()
{
    assert(list_lock_.isHeldBy(currentThread));
    return ptids_.size();
}

ustl::list<ptid>* PidWaits::getPtids()
{
    assert(list_lock_.isHeldBy(currentThread));
    return &ptids_;
}