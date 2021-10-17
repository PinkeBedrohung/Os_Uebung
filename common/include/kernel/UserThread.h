#pragma once

#include "Thread.h"
#include "UserProcess.h"

class UserThread : public Thread
{
private:
    int32 fd_;
    UserProcess *process_;
    uint32 terminal_number_;

public:
    UserThread(UserProcess *process, void* (*start_routine)(void*), void* args, bool is_first);
    ~UserThread();
    virtual void Run(); // not used
    UserProcess *getProcess();
};

