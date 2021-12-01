#include "PageFaultHandler.h"
#include "kprintf.h"
#include "Thread.h"
#include "ArchInterrupts.h"
#include "offsets.h"
#include "Scheduler.h"
#include "Loader.h"
#include "Syscall.h"
#include "ArchThreads.h"
#include "UserThread.h"
extern "C" void arch_contextSwitch();

const size_t PageFaultHandler::null_reference_check_border_ = PAGE_SIZE;

inline bool PageFaultHandler::checkPageFaultIsValid(size_t address, bool user,
                                                    bool present, bool switch_to_us)
{
  assert((user == switch_to_us) && "Thread is in user mode even though is should not be.");
  assert(!(address < USER_BREAK && currentThread->loader_ == 0) && "Thread accesses the user space, but has no loader.");
  assert(!(user && currentThread->user_registers_ == 0) && "Thread is in user mode, but has no valid registers.");

  if(address < null_reference_check_border_)
  {
    debug(PAGEFAULT, "Maybe you are dereferencing a null-pointer.\n");
  }
  else if(!user && address >= USER_BREAK)
  {
    debug(PAGEFAULT, "You are accessing an invalid kernel address.\n");
  }
  else if(user && address >= USER_BREAK)
  {
    debug(PAGEFAULT, "You are accessing a kernel address in user-mode.\n");
  }
  else if(present)
  {
    debug(PAGEFAULT, "You got a pagefault even though the address is mapped.\n");

    if(currentThread->getThreadType() == Thread::USER_THREAD)
    {
      debug(COW, "Copy Page / Set writeable\n");
      currentThread->handled_cow = true;
      /// TODO FORK: RC -2 Leads to KP with multithreaded fork (assert hits), two map the same page
      ArchMemory::copyPage(currentThread->loader_->arch_memory_, address);
      return true;
    }
  }
  else
  {
    // everything seems to be okay
    return true;
  }
  return false;
}

inline void PageFaultHandler::handlePageFault(size_t address, bool user,
                                          bool present, bool writing,
                                          bool fetch, bool switch_to_us)
{
  if (PAGEFAULT & OUTPUT_ENABLED)
    kprintfd("\n");
  debug(PAGEFAULT, "Address: %18zx - Thread %zu: %s (%p)\n",
        address, currentThread->getTID(), currentThread->getName(), currentThread);
  debug(PAGEFAULT, "Flags: %spresent, %s-mode, %s, %s-fetch, switch to userspace: %1d\n",
        present ? "    " : "not ",
        user ? "  user" : "kernel",
        writing ? "writing" : "reading",
        fetch ? "instruction" : "    operand",
        switch_to_us);
  //Uncomment the line below if you want to have detailed information about the thread registers.
  //ArchThreads::printThreadRegisters(currentThread, false);

  if (checkPageFaultIsValid(address, user, present, switch_to_us))
  {
    bool user_thread = currentThread->getThreadType() == Thread::USER_THREAD;
    if (user_thread)
    {
      const pointer virt_page_start_addr = address & ~(PAGE_SIZE - 1);
      const size_t page = virt_page_start_addr / PAGE_SIZE;
      const size_t page_offset = (size_t) ((USER_BREAK/PAGE_SIZE - 1) - page);
      UserThread *thread = (UserThread*) currentThread;
      if(present && currentThread->handled_cow)
      {
        currentThread->handled_cow = false;
        return;
      }
      else if (page_offset <= MAX_THREADS * MAX_THREAD_PAGES)
      {
          debug(PAGEFAULT, "STACK: Page offset: %zd\n", page_offset);
          debug(PAGEFAULT, "STACK: Stack base: %zd\n", thread->getStackBase());
          if((thread->getStackBase() + MAX_STACK_PAGES) < page_offset)
          {
              debug(PAGEFAULT, "ERROR: Lower guard page!\n");
              Syscall::exit(9997);
          }
          else if(page_offset < thread->getStackBase())
          {
              debug(PAGEFAULT, "ERROR: Upper guard page!\n");
              Syscall::exit(9998);
          }
          /// TODO MULTITHREADING: Growing stacks - Can't map stack of other thread -1 topic
          else if(page_offset > thread->getStackBase() &&  page_offset <= (thread->getStackBase() + MAX_STACK_PAGES))
          {
              debug(PAGEFAULT, "STACK: Add new page: %zd\n", page_offset);
              thread->growStack(page_offset);
              return;
          }
          else
          {
              assert(false);
          }
      }
    }   

    currentThread->loader_->loadPage(address);
  }
  else
  {
    // the page-fault seems to be faulty, print out the thread stack traces
    ArchThreads::printThreadRegisters(currentThread, true);
    currentThread->printBacktrace(true);
    if (currentThread->loader_)
      Syscall::exit(9999);
    else
      currentThread->kill();
  }
  debug(PAGEFAULT, "Page fault handling finished for Address: %18zx.\n", address);
}

void PageFaultHandler::enterPageFault(size_t address, bool user,
                                      bool present, bool writing,
                                      bool fetch)
{
  assert(currentThread && "You have a pagefault, but no current thread");
  //save previous state on stack of currentThread
  uint32 saved_switch_to_userspace = currentThread->switch_to_userspace_;

  currentThread->switch_to_userspace_ = 0;
  currentThreadRegisters = currentThread->kernel_registers_;
  ArchInterrupts::enableInterrupts();

  handlePageFault(address, user, present, writing, fetch, saved_switch_to_userspace);

  ArchInterrupts::disableInterrupts();
  currentThread->switch_to_userspace_ = saved_switch_to_userspace;
  if (currentThread->switch_to_userspace_)
    currentThreadRegisters = currentThread->user_registers_;
}
