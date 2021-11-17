#pragma once

#include "Thread.h"
#include "Condition.h"
#include "UserProcess.h"

#define MAX_STACK_PAGES 20
#define MAX_STACK_ARG_PAGES 256

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
    void execStackSetup(char** argv , ustl::list<int> &chars_per_arg);

    
    bool chainJoin(size_t thread);
    UserThread* join_;
    Condition alive_cond_;
    bool to_cancel_;
    Mutex cancel_lock_;
    bool first_thread_;
private:
    void createThread(void* entry_function);
    size_t page_offset_;
    size_t stack_base_nr_;
    //ustl::list<size_t> page_offsets_;
};
