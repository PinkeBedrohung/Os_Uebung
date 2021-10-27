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
        , alive_cond_(&process->alive_lock_, "alive_cond_")
{
    setThreadID(Scheduler::instance()->getNewTID());
    loader_ = process_->getLoader();
    createThread(loader_->getEntryFunction());
    join_ = NULL;
}

UserThread::UserThread(UserProcess* process, size_t* tid, void* (*routine)(void*), void* args, void* entry_function) :
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD)
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
        , alive_cond_(&process->alive_lock_, "alive_cond_")
{
    setThreadID(Scheduler::instance()->getNewTID());
    loader_ = process_->getLoader();
    createThread(entry_function);
    join_ = NULL;


    *tid = this->getTID();

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine,(size_t)args);
    ArchThreads::atomic_set(this->user_registers_->rdi, (size_t)routine);
    ArchThreads::atomic_set(this->user_registers_->rsi, (size_t)args);
}

void UserThread::createThread(void* entry_function)
{
    size_t page_for_stack = PageManager::instance()->allocPPN();
    page_offset_ = getTID();

    size_t stack_address = (size_t) (USER_BREAK - sizeof(pointer) - (PAGE_SIZE * page_offset_));
    
    bool vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE - page_offset_ - 1, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreads::createUserRegisters(user_registers_, entry_function, (void*) stack_address, getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    
    debug(USERTHREAD, "ctor: Done loading %s\n", name_.c_str());
    cpu_start_rdtsc = ArchThreads::rdtsc();
    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    switch_to_userspace_ = 1;
    ArchThreads::printThreadRegisters(this);
}

UserThread::UserThread(UserThread &thread, UserProcess* process) : 
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD),
        fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
        , alive_cond_(&process->alive_lock_, "alive_cond_")
{
    loader_ = process->getLoader();

    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                        (void*) (USER_BREAK - sizeof(pointer)),
                                        getKernelStackStartPointer());   
    
    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));
    
    copyRegisters(&thread);
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
    assert(Scheduler::instance()->isCurrentlyCleaningUp());

    debug(USERTHREAD, "~UserThread - TID %zu\n", getTID());
    process_->alive_lock_.acquire();
    alive_cond_.broadcast();
    process_->alive_lock_.release();

    process_->removeThread(this);

    // TODO: Unmap the mapped pages

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