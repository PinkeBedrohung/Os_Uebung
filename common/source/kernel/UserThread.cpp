#include "UserThread.h"
#include "ProcessRegistry.h"
#include "kprintf.h"
#include "Console.h"
#include "Loader.h"
#include "VfsSyscall.h"
#include "File.h"
#include "ArchMemory.h"
#include "PageManager.h"
#include "ArchThreads.h"
#include "offsets.h"
#include "UserProcess.h"

UserThread::UserThread(UserProcess* process) :
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD)
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
        , alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock")
{
    setThreadID(Scheduler::instance()->getNewTID());
    loader_ = process_->getLoader();
    createThread(loader_->getEntryFunction());
    join_ = NULL;
    to_cancel_ = false;
    first_thread_ = false;
    retval_ = 0;
    is_joinable_ = JOINABLE;
}

UserThread::UserThread(UserProcess* process,  void* (*routine)(void*), void* args, void* entry_function) :
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD)
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
        , alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock")
{
    setThreadID(Scheduler::instance()->getNewTID());
    loader_ = process_->getLoader();
    createThread(entry_function);
    join_ = NULL;
    to_cancel_ = false;
    retval_ = 0;
    is_joinable_ = JOINABLE;

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine,(size_t)args);
    user_registers_->rdi = (size_t)routine;
    user_registers_->rsi = (size_t)args;
}

void UserThread::createThread(void* entry_function)
{
    size_t page_for_stack = PageManager::instance()->allocPPN();
    page_offset_ = getTID() + 1; //guard pages

    size_t stack_address = (size_t) (USER_BREAK - sizeof(pointer) - (PAGE_SIZE * page_offset_));
    
    bool vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE - page_offset_ - 1, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreads::createUserRegisters(user_registers_, entry_function, (void*) stack_address, getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    
    debug(USERTHREAD, "ctor: Done loading %s\n", name_.c_str());
    
    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    switch_to_userspace_ = 1;
    ArchThreads::printThreadRegisters(this);
}

UserThread::UserThread(UserThread &thread, UserProcess* process) : 
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD),
        fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
        , alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock")
{
    loader_ = process->getLoader();

    to_cancel_ = false;
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                        (void*) (USER_BREAK - sizeof(pointer)),
                                        getKernelStackStartPointer());   
    
    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    page_offset_ = thread.page_offset_;
    is_joinable_ = thread.is_joinable_;
    copyRegisters(&thread);
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
    assert(Scheduler::instance()->isCurrentlyCleaningUp());

    debug(USERTHREAD, "~UserThread - TID %zu\n", getTID());
    

    if (process_->getLoader() != nullptr && !first_thread_)
        loader_->arch_memory_.unmapPage(USER_BREAK / PAGE_SIZE - page_offset_ - 1);

    if (process_->getNumThreads() == 0)
        delete process_;
}

void UserThread::Run()
{
    debug(USERTHREAD, "Run: Fail-safe kernel panic - you probably have forgotten to set switch_to_userspace_ = 1\n");
    assert(false);
}

UserProcess *UserThread::getProcess(){
    return process_;
}

void UserThread::copyRegisters(UserThread* thread)
{
    memcpy(user_registers_, thread->user_registers_, sizeof(ArchThreadRegisters));
    //memcpy(&kernel_stack_[1], &thread->kernel_stack_[1], (sizeof(kernel_stack_)-2)/sizeof(uint32));
    //memcpy(kernel_registers_, thread->kernel_registers_, sizeof(ArchThreadRegisters));
}

bool UserThread::chainJoin(size_t thread)
{
    UserThread* joiner = (UserThread*) ((UserThread*)currentThread)->getProcess()->getThread(thread);
    size_t caller = currentThread->getTID();

    while(joiner != NULL)
    {
        if(joiner->getTID() == caller)
            return true;

        joiner = joiner->join_;
    }
    return false;
}

bool UserThread::isStateJoinable()
{
    if(is_joinable_ == JOINABLE)
        return true;
    else
        return false;
}

size_t UserThread::setStateDetached()
{
    if(isStateJoinable())
    {
        is_joinable_ = DETACHED;
        return 0;
    }
    else
        return -1;
}

