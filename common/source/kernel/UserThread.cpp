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
    ArchThreads::printThreadRegisters(this);
}

UserThread::UserThread(UserThread &thread, UserProcess* process) : 
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD),
        fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
{
    //(void)thread;
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

    delete working_dir_;
    working_dir_ = 0;

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