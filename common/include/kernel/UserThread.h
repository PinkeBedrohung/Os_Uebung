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
    UserThread(UserProcess *process);
    UserThread(UserThread &thread, UserProcess* process = NULL);
    UserThread(UserProcess *process, size_t* tid, void* (*start_routine)(void*), void* args, void* entry_function);
    ~UserThread();
    virtual void Run(); // not used
    UserProcess *getProcess();
    void copyRegisters(UserThread *thread);
    

private:
    void createThread(void* entry_function);
    size_t page_offset_;
};
