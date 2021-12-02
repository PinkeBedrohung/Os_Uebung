#pragma once

#include "types.h"
#include <ulist.h>
#include "IdleThread.h"
#include "CleanupThread.h"
#include "Mutex.h"

class Thread;
class Mutex;
class SpinLock;
class Lock;

class Scheduler
{
  public:
    static Scheduler *instance();

    void addNewThread(Thread *thread);
    void sleep();
    void wake(Thread *thread_to_wake);
    void yield();
    void printThreadList();
    void printStackTraces();
    void printLockingInformation();
    bool isSchedulingEnabled();
    bool isCurrentlyCleaningUp();
    void incTicks();
    uint32 getTicks();

    size_t average_rdtsc_ = 0;
    /**
     * NEVER EVER EVER CALL THIS METHOD OUTSIDE OF AN INTERRUPT CONTEXT
     * this is the method that decides which threads will be scheduled next
     * it is called by either the timer interrupt handler or the yield interrupt handler
     * and changes the global variables currentThread and currentThreadRegisters
     * @return 1 if the InterruptHandler should switch to Usercontext or 0 if we can stay in Kernelcontext
     */
    uint32 schedule();
    unsigned long long scheduledtime = 0;
    unsigned long long rdtsc = 0;
    size_t getNewTID();

  protected:
    friend class IdleThread;
    friend class CleanupThread;

    void cleanupDeadThreads();

  private:
    Scheduler();

    /**
     * Scheduler internal lock abstraction method
     * locks the thread-list against concurrent access by prohibiting a thread switch
     * don't call this from an Interrupt-Handler, since Atomicity won't be guaranteed
     */
    void lockScheduling();

    /**
     * Scheduler internal lock abstraction method
     * unlocks the thread-list
     */
    void unlockScheduling();

    
    

    static Scheduler *instance_;

    typedef ustl::list<Thread*> ThreadList;
    ThreadList threads_;
    Mutex num_threads_lock_;
    size_t num_threads_;

    size_t block_scheduling_;

    size_t ticks_;

    IdleThread idle_thread_;
    CleanupThread cleanup_thread_;
};
