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

UserThread::UserThread(UserProcess* process, size_t* tid, void* (*routine)(void*), void* args, void* entry_function) :
        Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD)
        , fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber())
{
    loader_ = process_->getLoader();
    createThread(entry_function);

    *tid = this->getTID();

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine,(size_t)args);
    ArchThreads::atomic_set(this->user_registers_->rdi, (size_t)routine);
    ArchThreads::atomic_set(this->user_registers_->rsi, (size_t)args);
    
}

void UserThread::allocatePage(char const *arg[], Loader* loader, int32_t fd)
{
    //loader_ = process_->getLoader();
    size_t page_for_stack = PageManager::instance()->allocPPN();
    bool vpn_mapped = loader->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE -1 , page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
    void* virtual_stack_address = (void*) ((USER_BREAK / PAGE_SIZE -1) * PAGE_SIZE );
    char** new_arg;
    size_t arg_counter = 1; // nullpointer not counted
    size_t member_counter = 1;
    void* current_v_address;
   
    // ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    
    debug(USERTHREAD,"entry function = %p \n" , loader->getEntryFunction());
    
    if (arg == NULL)
    {
    /*user_registers_->rdi = arg_counter;
    user_registers_->rsi = (size_t)new_arg;
    */
    user_registers_->rbp = (size_t)((size_t*)virtual_stack_address);
    user_registers_->rsp = (size_t)((size_t*)virtual_stack_address);
    
    
        debug(USERTHREAD,"arg are NULL\n");

    }
    else 
    {
        for(size_t i = 0; arg[i] != NULL ; i++)
        {    
            arg_counter++;
            for(size_t pi = 0 ; arg[i][pi]!= '\0' ; pi++)
            {
                member_counter++;
            } 
        }     
        size_t offset = (member_counter + arg_counter) * sizeof(char) + (arg_counter+1) * sizeof(void*);
        arg_counter++;
        
        
        new_arg = (char**)virtual_stack_address - offset;
        current_v_address = (void*)((size_t)((size_t*)virtual_stack_address) - offset + arg_counter * sizeof(void*));

        for(size_t i = 0; i < arg_counter; i++ )
        {
        
        new_arg[i] = (char*)current_v_address;
        member_counter = 0;
            for(size_t pi = 0 ; arg[i][pi]!= '\0' ; pi++)
            {
                member_counter++;
            }
            member_counter++;
            
            for(size_t pi = 0 ; pi < member_counter ; pi++)
            {
            
            new_arg[i][pi] = arg[i][pi];
            }          
            current_v_address = (void*)((size_t)((size_t*)current_v_address) + sizeof(char)*member_counter);    
            
        }
        user_registers_->rdi = arg_counter;
        user_registers_->rsi = (size_t)new_arg;
        user_registers_->rbp = (size_t)((size_t*)virtual_stack_address) - offset -1;
        user_registers_->rsp = (size_t)((size_t*)virtual_stack_address) - offset -1;
    }
    user_registers_->rip = (size_t)loader->getEntryFunction();
    name_ = process_->getFilename();
    fd_ = fd;
    switch_to_userspace_ = 1;

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