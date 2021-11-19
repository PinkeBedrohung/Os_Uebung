#pragma once
#include <umap.h>
#include "types.h"
#include "Mutex.h"
#include "Lock.h"


class Thread;

class Semaphore
{
    friend class Scheduler;
    friend class Condition;

    public:
        static const size_t WAIT = 1;
        Semaphore(size_t permits);
        void acquire();
        void release();

    private:
        Mutex permits_lock;
        size_t permits_;
        ustl::map<size_t, size_t> tid_wait_map; //map of waiting tids

};

