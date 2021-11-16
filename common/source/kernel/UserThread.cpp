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
#include "ulist.h"

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

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine,(size_t)args);
    user_registers_->rdi = (size_t)routine;
    user_registers_->rsi = (size_t)args;
    
}

void UserThread::allocatePage(char** argv, int argv_size)
{
    //loader_ = process_->getLoader();
    //ArchMemory arguments;
    //arguments.mapPage(kernel_stack_);
   // size_t page_offset = page_offset_;

    size_t page_for_stack = PageManager::instance()->allocPPN();
    bool vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE -1 , page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
    vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE -2 , page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
    
    void* virtual_stack_address = (void*) ((USER_BREAK / PAGE_SIZE -1) * PAGE_SIZE );
    debug(MAIN,"%p \n",virtual_stack_address);
    char** new_arg;
    size_t arg_counter = 1; // nullpointer not counted
    size_t member_counter = 1;
    void* current_v_address;

    delete user_registers_;
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(), (void *)((size_t)(virtual_stack_address) + PAGE_SIZE - sizeof(pointer)), getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);

    debug(USERTHREAD, "entry function = %p \n", loader_->getEntryFunction());
    if (argv == NULL)
    {
        /*user_registers_->rdi = arg_counter;
    user_registers_->rsi = (size_t)new_arg;
    */
        //user_registers_->rbp = (size_t)((size_t*)virtual_stack_address)-1;
        //user_registers_->rsp = (size_t)((size_t*)virtual_stack_address)-1;
        //ArchThreads::createUserRegisters(user_registers_,loader->getEntryFunction(),virtual_stack_address, getKernelStackStartPointer());
        //user_registers_->rdi = (size_t)((size_t*)virtual_stack_address)-1;
        //user_registers_->rsi = arg_counter;
        //ArchThreads::atomic_set(this->user_registers_->rdi, (size_t)routine);
        //ArchThreads::atomic_set(this->user_registers_->rsi, (size_t)args);
        //user_registers_->rsi = (size_t)new_arg;

        debug(USERTHREAD, "args are NULL\n");

    }
    else 
    {
        debug(EXEC,"args are NOT NULL \n");
        ustl::list<int> chars_per_arg;
        int char_counter;
        for (size_t i = 0; argv[i] != NULL; i++)
        {
            arg_counter++;
            char_counter = 1;
            for (size_t pi = 0; argv[i][pi] != '\0'; pi++)
            {
                char_counter++;
                member_counter++;
            }
            chars_per_arg.insert(chars_per_arg.end(), char_counter);
            debug(EXEC, "1\n");
        }     

        (void) argv_size;

        size_t offset = (member_counter + arg_counter) * sizeof(char) + (arg_counter+1) * sizeof(void*);
        arg_counter++;

        new_arg = (char**)virtual_stack_address - offset;
        current_v_address = (void*)((size_t)((size_t*)virtual_stack_address) - offset); //+ arg_counter * sizeof(void*));

        char** argv_pointer = (char**)current_v_address;

        ArchMemoryMapping m = loader_->arch_memory_.resolveMapping(((size_t)(argv_pointer))/PAGE_SIZE);
        ArchMemory::printMemoryMapping(&m);
        debug(EXEC, "PPN present: %ld\n",m.pt[m.pti].present);

        ArchMemoryMapping d = loader_->arch_memory_.resolveMapping(((size_t)(USER_BREAK)/PAGE_SIZE - 1));
        ArchMemory::printMemoryMapping(&d);
        debug(EXEC, "PPN present: %ld\n",d.pt[d.pti].present);

        size_t halt = 0;
        for (size_t i = 0; i < arg_counter; i++)
        {
            char **argv_element = (argv_pointer + i);
            *argv_element = (char *)((size_t)current_v_address + arg_counter + chars_per_arg.at(i));
            char *argv_inside = *argv_element;
            halt = (size_t)argv_inside;
            (void)halt;

            for (int char_index = 0; char_index < chars_per_arg.at(i); char_index++)
            {
                char* argv_char_pointer = *argv_element+char_index;
                *argv_char_pointer = argv[i][char_index];
                char argv_char = *argv_char_pointer;
                halt = (size_t)argv_char;
                (void)halt;
            }
        }      
        user_registers_->rdi = arg_counter;
        user_registers_->rsi = (size_t)new_arg;
        //user_registers_->rip = user_registers_->rip - (arg_counter) * member_counter * sizeof(void*)*200;
        //user_registers_->rbp = (size_t)((size_t*)virtual_stack_address) - offset -1;
        //user_registers_->rsp = (size_t)((size_t*)virtual_stack_address) - offset -1;
        
    }
   // user_registers_->rip = (size_t)loader->getEntryFunction();
  // user_registers_->rip = (size_t)virtual_stack_address;
    debug(USERTHREAD,"PML4 ======== %ld \n" , loader_->arch_memory_.page_map_level_4_);
    
    name_ = process_->getFilename();
    fd_ = process_->getFd();
    page_offset_ = (size_t)(current_v_address);
    //switch_to_userspace_ = 1;
   
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

