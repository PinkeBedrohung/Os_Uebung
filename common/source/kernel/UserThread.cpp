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

UserThread::UserThread(UserProcess *process) : Thread(process->getFsInfo(), process->getFilename(),
            Thread::USER_THREAD), fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber()),
            alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock")
{
    setThreadID(Scheduler::instance()->getNewTID());
    stack_base_nr_ = process->getAvailablePageOffset() + (4 + MAX_STACK_ARG_PAGES);
    loader_ = process_->getLoader();
    createThread(loader_->getEntryFunction());
    join_ = NULL;
    to_cancel_ = false;
    first_thread_ = false;
    retval_ = 0;
    is_joinable_ = JOINABLE;
    is_killed_ = false;
}

UserThread::UserThread(UserProcess *process, void *(*routine)(void *), void *args, void *entry_function) : 
            Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD), fd_(process->getFd()),
            process_(process), terminal_number_(process->getTerminalNumber()), alive_cond_(&process->alive_lock_, "alive_cond_"),
            cancel_lock_("cancel_lock")
{
    setThreadID(Scheduler::instance()->getNewTID());
    stack_base_nr_ = process->getAvailablePageOffset() + (4 + MAX_STACK_ARG_PAGES);
    loader_ = process_->getLoader();
    createThread(entry_function);
    join_ = NULL;
    to_cancel_ = false;
    retval_ = 0;
    is_joinable_ = JOINABLE;
    is_killed_ = false;

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine, (size_t)args);
    user_registers_->rdi = (size_t)routine;
    user_registers_->rsi = (size_t)args;
}

size_t UserThread::execStackSetup(char **argv, ustl::list<int> &chars_per_arg,size_t needed_pages, size_t argv_size)
{
    bool vpn_mapped;

    process_->clearAvailableOffsets();
    stack_base_nr_ = process_->getAvailablePageOffset() + (4 + MAX_STACK_ARG_PAGES);;
    
    size_t page_for_stack = PageManager::instance()->allocPPN();
    stack_page_ = USER_BREAK / PAGE_SIZE - stack_base_nr_ - 1;
    vpn_mapped = loader_->arch_memory_.mapPage(stack_page_, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreadRegisters *old_user_registers = user_registers_;
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(), (void *)(stack_page_ * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)), getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    delete old_user_registers;

    name_ = process_->getFilename();
    fd_ = process_->getFd();

    debug(USERTHREAD, "entry function = %p \n", loader_->getEntryFunction());

    if (argv != NULL)
    {
        size_t arg_page;
        // debug(EXEC,"UserThread neededpages = %d",needed_pages);
        for (size_t arg_page_ctr = 0; arg_page_ctr < needed_pages; arg_page_ctr++)
        {
            arg_page = PageManager::instance()->allocPPN();
            vpn_mapped = loader_->arch_memory_.mapPage(USER_BREAK / PAGE_SIZE - (2+arg_page_ctr), arg_page, 1);
            assert(vpn_mapped && "Virtual page for arguments was already mapped - this should never happen");
        }

        size_t arg_start_address = (USER_BREAK / PAGE_SIZE - 1) * PAGE_SIZE;
        debug(EXEC, "arg_start_address: 0x%lx \n", arg_start_address);

        char **stack_argv = (char **)((size_t)((size_t *)arg_start_address) - argv_size);

        uint16 written_char_counter = 0;
        uint16 arg_counter = chars_per_arg.size() + 1;
        debug(EXEC, "Argcounter: %d\n", arg_counter);

        for (size_t i = 0; i < (size_t)arg_counter - 1; i++)
        {
            stack_argv[i] = (char *)((size_t)stack_argv + arg_counter * sizeof(char *) + written_char_counter);
            written_char_counter += chars_per_arg.at(i);
            memcpy(stack_argv[i], argv[i], (size_t)chars_per_arg.at(i));
        }

        user_registers_->rdi = arg_counter - 1;
        user_registers_->rsi = (size_t)stack_argv; 
    }
    return 0;
}

void UserThread::createThread(void *entry_function)
{
    size_t page_for_stack = PageManager::instance()->allocPPN();

    stack_page_ = USER_BREAK / PAGE_SIZE - stack_base_nr_ - 1;
    bool vpn_mapped = loader_->arch_memory_.mapPage(stack_page_, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreads::createUserRegisters(user_registers_, entry_function, (void *)(stack_page_ * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)), getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);

    debug(USERTHREAD, "ctor: Done loading %s\n", name_.c_str());

    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    switch_to_userspace_ = 1;
    ArchThreads::printThreadRegisters(this);
}

UserThread::UserThread(UserThread &thread, UserProcess *process) : Thread(process->getFsInfo(), process->getFilename(),
            Thread::USER_THREAD), fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber()),
            alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock"), stack_base_nr_(thread.stack_base_nr_)
{
    setThreadID(Scheduler::instance()->getNewTID());
    debug(EXEC, "TID: %ld\n", getTID());

    loader_ = process->getLoader();
    to_cancel_ = false;

    is_killed_ = false;

    stack_base_nr_ = thread.stack_base_nr_;
    is_joinable_ = thread.is_joinable_;
    stack_page_ = USER_BREAK / PAGE_SIZE - stack_base_nr_ - 1;
    
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                     (void *)(stack_page_ * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)),
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

    process_->threads_lock_.acquire();
    process_->removeThread((Thread*) this);
    if (process_->getNumThreads() == 0)
    {        
        process_->threads_lock_.release();
        delete process_;
    }
    else
    {
        process_->threads_lock_.release();
    }
}

void UserThread::Run()
{
    debug(USERTHREAD, "Run: Fail-safe kernel panic - you probably have forgotten to set switch_to_userspace_ = 1\n");
    assert(false);
}

UserProcess *UserThread::getProcess()
{
    return process_;
}

void UserThread::copyRegisters(UserThread *thread)
{
    memcpy(user_registers_, thread->user_registers_, sizeof(ArchThreadRegisters));
    //memcpy(&kernel_stack_[1], &thread->kernel_stack_[1], (sizeof(kernel_stack_)-2)/sizeof(uint32));
    //memcpy(kernel_registers_, thread->kernel_registers_, sizeof(ArchThreadRegisters));
}

bool UserThread::chainJoin(size_t thread)
{
    ((UserThread *)currentThread)->getProcess()->threads_lock_.acquire();
    UserThread *joiner = (UserThread *)((UserThread *)currentThread)->getProcess()->getThread(thread);
    ((UserThread *)currentThread)->getProcess()->threads_lock_.release();
    size_t caller = currentThread->getTID();

    while (joiner != NULL)
    {
        if (joiner->getTID() == caller)
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

size_t UserThread::getStackBase()
{
    return stack_base_nr_;
}

size_t UserThread::getStackPage()
{
    return stack_page_;
}

void UserThread::growStack(size_t page_offset)
{
    size_t page_for_stack = PageManager::instance()->allocPPN();
    size_t virtual_page = USER_BREAK / PAGE_SIZE - page_offset - 1;
    bool vpn_mapped = loader_->arch_memory_.mapPage(virtual_page, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
    used_offsets_.push_back(page_offset);
}

ustl::list<size_t> UserThread::getUsedOffsets()
{
    return used_offsets_;
}

void UserThread::cleanupThread(size_t retval)
{
  process_->threads_lock_.acquire();
  if (is_killed_)
  {
      process_->threads_lock_.acquire();
      return;      
  }

  is_killed_ = true;

  if (process_->getLoader() != nullptr) //&& !first_thread_)
  {
      for (size_t page_offset : getUsedOffsets())
      {
          size_t page = USER_BREAK / PAGE_SIZE - page_offset - 1;

          debug(EXEC, "PAGE_OFFSET: %ld\n", page_offset);
          process_->getLoader()->arch_memory_.unmapPage(page);
      }

      process_->getLoader()->arch_memory_.unmapPage(getStackPage());      
  }
    
  process_->mapRetVals(getTID(), (void*) retval);
  
  if (join_ != NULL)
  {
    process_->alive_lock_.acquire();
    join_->alive_cond_.broadcast();
    process_->alive_lock_.release();
  }

  process_->freePageOffset(getStackBase() - (4 + MAX_STACK_ARG_PAGES));
  process_->threads_lock_.release();  
}