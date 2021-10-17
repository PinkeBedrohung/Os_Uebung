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
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber()){
    
    loader_ = process_->getLoader();
  

    size_t page_for_stack = PageManager::instance()->allocPPN();
    bool vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE - 1, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                    (void*) (USER_BREAK - sizeof(pointer)),
                                    getKernelStackStartPointer());

    ArchThreads::setAddressSpace(this, loader_->arch_memory_);

    debug(USERTHREAD, "ctor: Done loading %s\n", name_.c_str());

    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    switch_to_userspace_ = 1;
}

UserThread::UserThread(UserProcess* process, void* (*routine)(void*), void* args, bool is_first) :
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD)
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber()){
    
    loader_ = process_->getLoader();
    size_t page_for_stack = PageManager::instance()->allocPPN();
    //bool vpn_mapped = 
    bool vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE - 1, page_for_stack, 1);
    //assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
    
    
    size_t stack_offset = ArchThreads::getRand(1, 1024);
    void* stack_address_ = (void*) (USER_BREAK - sizeof(pointer) - (PAGE_SIZE) - stack_offset);
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(), stack_address_,
                                    getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    
    if(vpn_mapped) 
         debug(USERTHREAD, "Error: VPN already mapped\n");
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    if(is_first && args)
    {
        debug(USERTHREAD, "ATTENTION: First Thread\n");
        ArchThreads::atomic_set(this->user_registers_->rdi, (size_t)args);
        ArchThreads::atomic_set(this->user_registers_->rsi, 0);
    }
    else
    {
        debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine,(size_t)args);
        ArchThreads::atomic_set(this->user_registers_->rdi, (size_t)routine);
        ArchThreads::atomic_set(this->user_registers_->rsi, (size_t)args);
    }
    debug(USERTHREAD, "ctor: Done loading %s\n", name_.c_str());

    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
    assert(Scheduler::instance()->isCurrentlyCleaningUp());

    debug(USERTHREAD, "~UserThread - TID %zu\n", getTID());

    process_->remove_thread(this);

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