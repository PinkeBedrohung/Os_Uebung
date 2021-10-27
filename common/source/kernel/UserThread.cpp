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
{
    loader_ = process_->getLoader();
    createThread(loader_->getEntryFunction());
}

UserThread::UserThread(UserProcess* process,  void* (*routine)(void*), void* args, void* entry_function) :
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD)
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
{
    loader_ = process_->getLoader();
    createThread(entry_function);

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine,(size_t)args);
    ArchThreads::atomic_set(this->user_registers_->rdi, (size_t)routine);
    ArchThreads::atomic_set(this->user_registers_->rsi, (size_t)args);
}

void UserThread::createThread(void* entry_function)
{
    size_t page_for_stack = PageManager::instance()->allocPPN();
    page_offset_ = process_->created_threads_ + 1; //improve thread addresses 

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