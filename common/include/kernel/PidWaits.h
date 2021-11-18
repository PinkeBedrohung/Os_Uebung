#pragma once
#include "types.h"
#include "ulist.h"
#include "Mutex.h"
#include "Condition.h"
#include "debug.h"


struct ptid
{
    size_t pid_;
    size_t tid_;
};

class PidWaits
{
public:
    PidWaits(size_t pid);
    ~PidWaits();

    void addPtid(size_t pid, size_t tid);
    void removePtid(size_t pid, size_t tid);
    void signalReady();
    void waitUntilReady(size_t pid, size_t tid);
    size_t getPid();
    size_t getNumPtids();
    ustl::list<ptid>* getPtids();

    Mutex list_lock_;

private:
    size_t pid_to_wait_for_;
    Mutex cond_lock_;
    Condition pid_ready_cond_;
    ustl::list<ptid> ptids_;
};