#pragma once

#include "Thread.h"
#include "Condition.h"
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
    UserThread(UserProcess *process,  void* (*start_routine)(void*), void* args, void* entry_function);
    ~UserThread();
    virtual void Run(); // not used
    UserProcess *getProcess();
    void copyRegisters(UserThread *thread);
    bool chainJoin(size_t thread);
    UserThread* join_;
    Condition alive_cond_;
    bool to_cancel_;
    Mutex cancel_lock_;
    bool first_thread_;
    size_t retval_;
private:
    void createThread(void* entry_function);
    size_t page_offset_;
};
