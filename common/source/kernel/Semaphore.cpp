
#include "Semaphore.h"
#include "Thread.h"
#include "Scheduler.h"
#include "ArchThreads.h"

Semaphore::Semaphore(size_t permits) :  permits_lock("Semaphore::permits_lock") , permits_(permits) {}

void Semaphore::acquire()
{
    permits_lock.acquire();
    if(permits_ == 0)
    {
        permits_lock.release();
        tid_wait_map.insert(ustl::make_pair(currentThread->getTID(), WAIT));
        while(ArchThreads::testSetLock(tid_wait_map.at(currentThread->getTID()), 1))
        {
            Scheduler::instance()->yield();
        }
        permits_lock.acquire();
        permits_--;
        permits_lock.release();
        tid_wait_map.erase(currentThread->getTID());
    }
    else
    {
        permits_--;
        permits_lock.release();
    }
}

void Semaphore::release()
{
    permits_lock.acquire();
    permits_++;
    tid_wait_map.begin()->second = 0;
    permits_lock.release();
}
