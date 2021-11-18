#pragma once
#include "types.h"
#include "ulist.h"
#include "Mutex.h"
#include "Condition.h"

class PidWaits
{
public:
    PidWaits(size_t pid);
    ~PidWaits();

    void addTid(size_t tid);
    void removeTid(size_t tid);
    void signalReady();
    void waitUntilReady(size_t tid);
    size_t getPid();
    size_t getNumTids();

    Mutex list_lock_;

private:
    size_t pid_;
    Mutex cond_lock_;
    Condition pid_ready_cond_;
    ustl::list<size_t> tids_;
};