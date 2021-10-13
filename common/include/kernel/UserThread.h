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
    UserThread(UserProcess *process);
    ~UserThread();
    virtual void Run(); // not used
};

