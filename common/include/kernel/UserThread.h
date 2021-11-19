#pragma once

#include "Thread.h"
#include "Condition.h"
#include "UserProcess.h"
#include "ulist.h"

#define MAX_STACK_PAGES 64
#define MAX_STACK_ARG_PAGES 64
#define MAX_THREAD_PAGES MAX_STACK_PAGES + MAX_STACK_ARG_PAGES + 4
#define MAX_THREADS 2048

class UserProcess;

class UserThread : public Thread
{
private:
    int32 fd_;
    UserProcess *process_;
    uint32 terminal_number_;

public:

    enum JoinableState
    {
        JOINABLE, DETACHED
    };

    UserThread(UserProcess *process);
    UserThread(UserThread &thread, UserProcess* process = NULL);
    UserThread(UserProcess *process,  void* (*start_routine)(void*), void* args, void* entry_function);
    ~UserThread();
    virtual void Run(); // not used
    UserProcess *getProcess();
    void copyRegisters(UserThread *thread);


    bool checkIfLastThread();
    size_t getStackBase();
    size_t getStackPage();
    size_t getNumPages();
    void growStack(size_t page_offset);
    size_t execStackSetup(char** argv , ustl::list<int> &chars_per_arg, size_t needed_pages, size_t argv_size);
    bool chainJoin(size_t thread);
    bool isStateJoinable();
    size_t setStateDetached();
    UserThread* join_;
    Condition alive_cond_;
    bool to_cancel_;
    Mutex cancel_lock_;
    bool first_thread_;
    size_t retval_;
    int is_joinable_;

private:
    void createThread(void* entry_function);
    size_t stack_base_nr_;
    size_t stack_page_;
    ustl::list<size_t> used_offsets_;
};
