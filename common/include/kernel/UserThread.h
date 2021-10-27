#pragma once

#include "Thread.h"
#include "Condition.h"
#include "UserProcess.h"

class UserThread : public Thread
{
private:
    int32 fd_;
    UserProcess *process_;
    uint32 terminal_number_;

public:
    UserThread(UserProcess *process, size_t* tid, void* (*start_routine)(void*), void* args, void* entry_function);
    UserThread(UserProcess* process);
    ~UserThread();
    virtual void Run(); // not used
    UserProcess *getProcess();
    UserThread* join_;
    Condition alive_cond_;

private:
    void createThread(void* entry_function);
    size_t page_offset_;
};

