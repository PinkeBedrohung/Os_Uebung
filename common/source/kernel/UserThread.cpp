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
            alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock"), page_offset_(0)
{
    setThreadID(Scheduler::instance()->getNewTID());
    stack_base_nr_ = getTID();
    loader_ = process_->getLoader();
    createThread(loader_->getEntryFunction());
    join_ = NULL;
    to_cancel_ = false;
    first_thread_ = false;
    retval_ = 0;
    is_joinable_ = JOINABLE;
}

UserThread::UserThread(UserProcess *process, void *(*routine)(void *), void *args, void *entry_function) : 
            Thread(process->getFsInfo(), process->getFilename(), Thread::USER_THREAD), fd_(process->getFd()),
            process_(process), terminal_number_(process->getTerminalNumber()), alive_cond_(&process->alive_lock_, "alive_cond_"),
            cancel_lock_("cancel_lock"), page_offset_(0)
{
    setThreadID(Scheduler::instance()->getNewTID());
    stack_base_nr_ = getTID();
    loader_ = process_->getLoader();
    createThread(entry_function);
    join_ = NULL;
    to_cancel_ = false;
    retval_ = 0;
    is_joinable_ = JOINABLE;

    debug(USERTHREAD, "ATTENTION: Not first Thread\n, setting rdi:%zu , and rsi:%zu\n", (size_t)routine, (size_t)args);
    user_registers_->rdi = (size_t)routine;
    user_registers_->rsi = (size_t)args;
}

size_t UserThread::execStackSetup(char **argv, ustl::list<int> &chars_per_arg,size_t needed_pages, size_t argv_size)
{
    bool vpn_mapped;
    page_offset_ = 0;
    
    size_t page_for_stack = PageManager::instance()->allocPPN();
    size_t vpage_nr = USER_BREAK / PAGE_SIZE - (4 + MAX_STACK_ARG_PAGES) - (stack_base_nr_ * (MAX_STACK_PAGES + 1)) - page_offset_;
    vpn_mapped = loader_->arch_memory_.mapPage(vpage_nr, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreadRegisters *old_user_registers = user_registers_;
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(), (void *)(vpage_nr * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)), getKernelStackStartPointer());
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

size_t UserThread::getStackBaseNr()
{
    return stack_base_nr_;
}

size_t UserThread::getPageOffset()
{
    return page_offset_;
}

void UserThread::createThread(void *entry_function)
{
    size_t page_for_stack = PageManager::instance()->allocPPN();

    size_t vpage_nr = USER_BREAK / PAGE_SIZE - (4 + MAX_STACK_ARG_PAGES) - (stack_base_nr_ * (MAX_STACK_PAGES + 1)) - page_offset_;

    bool vpn_mapped = loader_->arch_memory_.mapPage(vpage_nr, page_for_stack, 1);
    assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");

    ArchThreads::createUserRegisters(user_registers_, entry_function, (void *)(vpage_nr * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)), getKernelStackStartPointer());
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);

    debug(USERTHREAD, "ctor: Done loading %s\n", name_.c_str());

    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    switch_to_userspace_ = 1;
    ArchThreads::printThreadRegisters(this);
}

UserThread::UserThread(UserThread &thread, UserProcess *process) : Thread(process->getFsInfo(), process->getFilename(),
            Thread::USER_THREAD), fd_(process->getFd()), process_(process), terminal_number_(process->getTerminalNumber()),
            alive_cond_(&process->alive_lock_, "alive_cond_"), cancel_lock_("cancel_lock"), page_offset_(0), stack_base_nr_(thread.getTID())
{
    setThreadID(Scheduler::instance()->getNewTID());
    debug(EXEC, "TID: %ld\n", getTID());

    loader_ = process->getLoader();
    to_cancel_ = false;

    size_t vpage_nr = USER_BREAK / PAGE_SIZE - (4 + MAX_STACK_ARG_PAGES) - (stack_base_nr_ * (MAX_STACK_PAGES + 1)) - page_offset_;
    
    ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                     (void *)(vpage_nr * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)),
                                     getKernelStackStartPointer());

    if (main_console->getTerminal(terminal_number_))
        setTerminal(main_console->getTerminal(terminal_number_));

    page_offset_ = thread.page_offset_;
    stack_base_nr_ = thread.stack_base_nr_;
    is_joinable_ = thread.is_joinable_;
    copyRegisters(&thread);
    ArchThreads::setAddressSpace(this, loader_->arch_memory_);
    switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
    assert(Scheduler::instance()->isCurrentlyCleaningUp());

    debug(USERTHREAD, "~UserThread - TID %zu\n", getTID());
    
    if (process_->getLoader() != nullptr) //&& !first_thread_)
    {
        for (size_t page_offset = 0; page_offset <= page_offset_; page_offset++)
        {
            size_t vpage_nr = USER_BREAK / PAGE_SIZE - (4 + MAX_STACK_ARG_PAGES) - (stack_base_nr_ * (MAX_STACK_PAGES + 1)) - page_offset_;

            debug(EXEC, "PAGE_OFFSET: %ld\n", page_offset_);
            loader_->arch_memory_.unmapPage(vpage_nr);
        }
    }
    if (process_->getNumThreads() == 0)
        delete process_;
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
    UserThread *joiner = (UserThread *)((UserThread *)currentThread)->getProcess()->getThread(thread);
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

