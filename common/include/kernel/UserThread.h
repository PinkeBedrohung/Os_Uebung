#pragma once

#include "Thread.h"
#include "UserProcess.h"

class UserProcess;

class UserThread : public Thread
{
private:
    int32 fd_;
    UserProcess *process_;
    uint32 terminal_number_;

public:
    UserThread(UserProcess *process, bool forked = false);
    UserThread(UserThread &thread, UserProcess* process = NULL);
    ~UserThread();
    virtual void Run(); // not used
    UserProcess *getProcess();
};

