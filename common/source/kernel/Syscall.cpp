#include "offsets.h"
#include "Syscall.h"
#include "syscall-definitions.h"
#include "Terminal.h"
#include "debug_bochs.h"
#include "VfsSyscall.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "ProcessRegistry.h"
#include "File.h"
#include "ArchMemory.h"
#include "ArchThreads.h"
#include "Loader.h"
#include "ArchThreads.h"

size_t Syscall::syscallException(size_t syscall_number, size_t arg1, size_t arg2, size_t arg3, size_t arg4, size_t arg5)
{
  size_t return_value = 0;
  debug(SYSCALL, "CurrentThread %ld to_cancel_: %d\n",((UserThread*)currentThread)->getTID(), (int)((UserThread*)currentThread)->to_cancel_);
  /// TODO MULTITHREADING: RC -1
  if(((UserThread*)currentThread)->to_cancel_)
  {
    /// TODO MULTITHREADING: This does not save retval?
    currentThread->kill();
    return 0;
  }

  if ((syscall_number != sc_sched_yield) && (syscall_number != sc_outline)) // no debug print because these might occur very often
  {
    debug(SYSCALL, "Syscall %zd called with arguments %zd(=%zx) %zd(=%zx) %zd(=%zx) %zd(=%zx) %zd(=%zx)\n",
          syscall_number, arg1, arg1, arg2, arg2, arg3, arg3, arg4, arg4, arg5, arg5);
  }

  switch (syscall_number)
  {
    case sc_sched_yield:
      Scheduler::instance()->yield();
      break;
    case sc_createprocess:
      return_value = createprocess(arg1, arg2);
      break;
    case sc_exit:
      exit(arg1);
      break;
    case sc_write:
      return_value = write(arg1, arg2, arg3);
      break;
    case sc_read:
      return_value = read(arg1, arg2, arg3);
      break;
    case sc_open:
      return_value = open(arg1, arg2);
      break;
    case sc_close:
      return_value = close(arg1);
      break;
    case sc_outline:
      outline(arg1, arg2);
      break;
    case sc_trace:
      trace();
      break;
    case sc_pthread_create:
      return_value = createThread(arg1, arg2, arg3, arg4, arg5);
      break;
    case sc_pthread_cancel:
      return_value = cancelThread(arg1);
      break;
    case sc_pthread_exit:
      exitThread(arg1);
      break;
    case sc_pthread_join:
      return_value = joinThread(arg1, (void**)arg2);
      break;
    case sc_pthread_detach:
      return_value = detachThread(arg1);
      break;
    case sc_clock:
      return_value = clock();
      break;
    case sc_sleep:
      return_value = sleep((unsigned int) arg1);
      break;
    case sc_pthread_setcancelstate:
      return_value = setCancelState((size_t)arg1, (size_t)arg2);
      break;
    case sc_pthread_setcanceltype:
      return_value = setCancelType((size_t)arg1, (size_t)arg2);
      break;
    case sc_pseudols:
      VfsSyscall::readdir((const char*) arg1);
      break;
    case sc_fork:
      return_value = fork();
      break;
    case sc_exec:
      return_value = exec((const char *)arg1, (char const**)arg2);
      break;
    case  sc_waitpid:
      return_value = waitpid(arg1, arg2, arg3);
      break;
    case  sc_getpid:
      return_value = getpid();
      break;
    default:
      kprintf("Syscall::syscall_exception: Unimplemented Syscall Number %zd\n", syscall_number);
  }
  if(((UserThread*)currentThread)->to_cancel_)
  {
    currentThread->kill();
    return 0;
  }
  return return_value;
}

void Syscall::exit(size_t exit_code)
{
  debug(SYSCALL, "Syscall::EXIT: called, exit_code: %zd\n", exit_code);
  
  UserThread *currentUserThread = (UserThread *)currentThread;

  ProcessRegistry::instance()->lockLists();
  ProcessExitInfo pexit_info(exit_code, currentUserThread->getProcess()->getPID());
  ProcessRegistry::instance()->makeZombiePID(pexit_info.pid_);
  ProcessRegistry::instance()->addExitInfo(pexit_info);
  ProcessRegistry::instance()->signalPidAvailable(pexit_info.pid_);
  ProcessRegistry::instance()->unlockLists();
  debug(WAIT_PID, "Process exited - Exit_val: %ld, PID: %ld\n", pexit_info.exit_val_, pexit_info.pid_);

  UserProcess* current_process = currentUserThread->getProcess(); 
  current_process->cancelNonCurrentThreads(currentThread);
  currentThread->kill();
}

size_t Syscall::write(size_t fd, pointer buffer, size_t size)
{
  //WARNING: this might fail if Kernel PageFaults are not handled
  if ((buffer >= USER_BREAK) || (buffer + size > USER_BREAK))
  {
    return -1U;
  }

  size_t num_written = 0;

  if (fd == fd_stdout) //stdout
  {
    debug(SYSCALL, "Syscall::write: %.*s\n", (int)size, (char*) buffer);
    kprintf("%.*s", (int)size, (char*) buffer);
    num_written = size;
  }
  else
  {
    num_written = VfsSyscall::write(fd, (char*) buffer, size);
  }
  return num_written;
}

size_t Syscall::read(size_t fd, pointer buffer, size_t count)
{
  if ((buffer >= USER_BREAK) || (buffer + count > USER_BREAK))
  {
    return -1U;
  }

  size_t num_read = 0;

  if (fd == fd_stdin)
  {
    //this doesn't! terminate a string with \0, gotta do that yourself
    num_read = currentThread->getTerminal()->readLine((char*) buffer, count);
    debug(SYSCALL, "Syscall::read: %.*s\n", (int)num_read, (char*) buffer);
  }
  else
  {
    num_read = VfsSyscall::read(fd, (char*) buffer, count);
  }
  return num_read;
}

size_t Syscall::close(size_t fd)
{
  return VfsSyscall::close(fd);
}

size_t Syscall::open(size_t path, size_t flags)
{
  if (path >= USER_BREAK)
  {
    return -1U;
  }
  return VfsSyscall::open((char*) path, flags);
}

void Syscall::outline(size_t port, pointer text)
{
  //WARNING: this might fail if Kernel PageFaults are not handled
  if (text >= USER_BREAK)
  {
    return;
  }
  if (port == 0xe9) // debug port
  {
    writeLine2Bochs((const char*) text);
  }
}

size_t Syscall::createprocess(size_t path, size_t sleep)
{
  // THIS METHOD IS FOR TESTING PURPOSES ONLY!
  // AVOID USING IT AS SOON AS YOU HAVE AN ALTERNATIVE!

  // parameter check begin
  if (path >= USER_BREAK)
  {
    return -1U;
  }
  debug(SYSCALL, "Syscall::createprocess: path:%s sleep:%zd\n", (char*) path, sleep);
  ssize_t fd = VfsSyscall::open((const char*) path, O_RDONLY);
  if (fd == -1)
  {
    return -1U;
  }
  VfsSyscall::close(fd);
  // parameter check end

  size_t process_count = ProcessRegistry::instance()->processCount();
  ProcessRegistry::instance()->createProcess((const char*) path);
  if (sleep)
  {
    while (ProcessRegistry::instance()->processCount() > process_count) // please note that this will fail ;)
    {
      Scheduler::instance()->yield();
    }
  }
  return 0;
}

void Syscall::trace()
{
  currentThread->printBacktrace();
}

size_t Syscall::fork()
{
  int retval = 0;
  
  UserThread *currentUserThread = (UserThread *)currentThread;
  UserProcess *currentProcess = currentUserThread->getProcess();
  UserProcess *new_process = new UserProcess(*currentProcess, currentUserThread, &retval);

  if(retval != -1)
  {
    ProcessRegistry::instance()->createProcess(new_process);  
    return new_process->getPID();
  }
  else
  {
    delete new_process;
  }

  return -1;
}

size_t Syscall::createThread(size_t thread, size_t attr, size_t start_routine, size_t arg, size_t entry_function)
{
  if((size_t)start_routine >= USER_BREAK || (size_t)arg >= USER_BREAK || thread == 0 || start_routine == 0 || entry_function == 0 || entry_function >= USER_BREAK) return -1U;
  if((size_t)thread < USER_BREAK && (size_t)attr < USER_BREAK && (size_t)start_routine < USER_BREAK)
  {
    UserProcess* uprocess = ((UserThread*)currentThread)->getProcess();
    return uprocess->createUserThread((size_t*) thread, (void* (*)(void*))start_routine, (void*) arg, (void*) entry_function);
  }

    return -1U;
  
}

void Syscall::exitThread(size_t retval)
{
  //SOLVED: when a thread is cancelled, store -1 in retval so join can also return -1 as PTHREAD_CANCELED
  //((UserThread*)currentThread)->retval_ = retval;
  ((UserThread*)currentThread)->getProcess()->mapRetVals(currentThread->getTID(), (void*) retval);

  currentThread->kill();
}

size_t Syscall::clock()
{
  /// TODO OTHER: (Clock) Clock is about CPU time, not total time, so calculating now - start is incorrect
  unsigned long long rdtsc = ArchThreads::rdtsc(); //now
  unsigned long long difference = rdtsc - ((UserThread*)currentThread)->getProcess()->cpu_start_rdtsc;
  //debug(SYSCALL,"Difference: %lld\n", difference);
  //debug(SYSCALL,"rdtsc: %lld\n", rdtsc);
  size_t retval = (difference)/(Scheduler::instance()->average_rdtsc_/(54925439/1000));
  //debug(SYSCALL,"retval: %ld\n", retval);
  return (size_t) retval; //clock ticks
  //The value returned is expressed in clock ticks.
}

size_t Syscall::sleep(unsigned int seconds)
{
  /// Reasons for inaccuracy are in the calculation of average_rdtsc_
  unsigned long long current_rdtsc = ArchThreads::rdtsc();
  //debug(SYSCALL,"rdtsc time: %lld\n", current_rdtsc);
  unsigned long long additional_time = (seconds*1000)*(Scheduler::instance()->average_rdtsc_/(54925439/1000000));
  unsigned long long expected_time = current_rdtsc + additional_time; //start rdtsc + additional time
  currentThread->time_to_sleep_ = expected_time;
  //debug(SYSCALL,"expected time: %lld\n", expected_time);
  currentThread->setState(USleep);
  Scheduler::instance()->yield();
  return 0;
} 

int Syscall::exec(const char *path, char const* arg[])
{
  /// TODO EXEC: -1 Params why a path dereference for the check?
  if ((size_t)*path >= USER_BREAK)
  {
    return -1;
  }

  /// This check does nothing
  if(((UserThread*)currentThread)->getProcess() == 0)
  {
    return -1;
  }

  return ((UserThread *)currentThread)->getProcess()->replaceProcessorImage(path, arg);

}

size_t Syscall::joinThread(size_t thread, void** value_ptr)
{
  UserThread* calling_thread = ((UserThread*)currentThread);
  UserProcess* current_process = ((UserThread*)currentThread)->getProcess(); 
  UserThread* thread_to_join = (UserThread*)(current_process->getThread(thread));


  if((uint64)value_ptr >= USER_BREAK && value_ptr != NULL)
  {
    //debug(SYSCALL, "Return value address not in user space")
    return (size_t) -1U;
  }


  if(thread == calling_thread->getTID())
  {
    //debug(SYSCALL, "Thread is calling join on itselt")
    return (size_t) -1U;
  }

   // TODO MULTITHREADING: RC -1
  if(thread_to_join != NULL && !thread_to_join->isStateJoinable())
  {
    return (size_t) -1U;
  }


  
  if(thread_to_join != NULL)
  {
    if(calling_thread->chainJoin(thread_to_join->getTID()))
    {
      return (size_t) -1U;
    }

    /// TODO MULTITHREADING: RC -1
    if (thread_to_join->join_ != NULL)
    {
      return (size_t) -1U;
    }

    //current_process->threads_lock_.acquire();
    /// TODO MULTITHREADING: Severe RC and no CV?! -2
    thread_to_join->join_ = calling_thread;
    Scheduler::instance()->sleep();
    thread_to_join->join_ = NULL;
  }
  //else 
    //current_process->retvals_lock_.release();

  current_process->retvals_lock_.acquire();
  if(current_process->retvals_.find(thread) != current_process->retvals_.end()
    && value_ptr != NULL)
  /// TODO MULTITHREADING: Other -1 Pagefault with Kernel Lock -> Maybe remove from retvals_?
    *value_ptr = current_process->retvals_.at(thread);
  current_process->retvals_lock_.release();

  /// TODO MULTITHREADING: NonStd -1 Returning 0 if thread to join is not found
  return (size_t) 0;
}

// when implementing asynchronous, cancellation points: Scheduler , page fault
size_t Syscall::cancelThread(size_t tid)
{
  UserProcess* process = ((UserThread*)currentThread)->getProcess();
  /// Mapping retval before thread is actually dead? What if thread calls pthread_exit(-1), it gets identified as cancelled?
  ((UserThread*)currentThread)->getProcess()->mapRetVals(currentThread->getTID(), (void*) -1);
  debug(SYSCALL, "Calling cancelUserThread on Thread: %ld\n", tid);
  return process->cancelUserThread(tid);
}

size_t Syscall::detachThread(size_t tid)
{
  UserProcess* current_process = ((UserThread*)currentThread)->getProcess(); 
  UserThread* thread = (UserThread*)(current_process->getThread(tid));
  /// TODO MULTITHREADING: NonStd -1 Should return -1 if thread is not found, here we access a nullptr
  return thread->setStateDetached();
}

size_t Syscall::waitpid(size_t pid, pointer status, size_t options)
{
  if(status >= USER_BREAK)
  {
    return (size_t) -1UL;
  }

  // Only the default and WEXITED options are implemented - they are the same
  const int WEXITED = 4;
  if(!(options == WEXITED || options == 0))
  {
    return (size_t) -1UL;
  }

  size_t check_pid = pid;
  bool delete_entry = false;

  size_t curr_pid = ((UserThread *)currentThread)->getProcess()->getPID();
  size_t curr_tid = currentThread->getTID();
  ProcessRegistry *pr_instance = ProcessRegistry::instance();

  // Waiting for his own pid is not allowed and pid=0 pid<-1 functionality is not implemented
  if(pid == curr_pid || pid == 0 || (ssize_t)pid < (ssize_t)-1)
  {
    return (size_t) -1UL;
  }

  //Deadlock check
  pr_instance->lockLists();

  if(pid != (size_t) -1UL && pr_instance->checkPidWaitsHasDeadlock(curr_pid, pid))
  {
    pr_instance->unlockLists();
    return (size_t) -1UL;
  }

  // Wait for any terminated process
  if(pid == (size_t) -1UL)
  {
    
    size_t ret_pid = pr_instance->getFirstZombiePID();

    // Found a terminated pid
    if(ret_pid != (size_t) -1UL)
    {
      pr_instance->pid_waits_lock_.acquire();
      PidWaits* pid_waits;
      for (auto itr = pr_instance->pid_waits_.begin(); itr != pr_instance->pid_waits_.end(); itr++)
      {
        if(ret_pid == (*itr)->getPid())
        {
          pid_waits = *itr;
          break;
        }
      }

      pid_waits->list_lock_.acquire();
      if(pid_waits->getNumPtids() == 0)
      {
        delete_entry = true;
      }

      debug(WAIT_PID, "Thread:%ld tries to get Exit info of Process PID %ld with -1\n", curr_tid, ret_pid);
      pid_waits->list_lock_.release();
      ProcessExitInfo pexit(pr_instance->getExitInfo(ret_pid, delete_entry));
      pr_instance->pid_waits_lock_.release();

      /// TODO EXEC: (Waitpid) Other -2 PF with kernel lock (Happens thrice in this function)
      if(status != NULL)
        *((int*)status) = (pexit.exit_val_&0xFF) + (1<<8);

      debug(WAIT_PID, "Thread:%ld found terminated Process PID: %ld with -1\n", curr_tid, pexit.pid_);
      pr_instance->unlockLists();
      return pexit.pid_;
    }

    // No Process has terminated yet so the thread has to wait for a process in use
    pr_instance->unlockLists();
    pr_instance->pid_waits_lock_.acquire();
    debug(WAIT_PID, "Thread:%ld waits for any terminated Process\n", curr_tid);
    check_pid = pr_instance->getPidAvailable();
    debug(WAIT_PID, "Thread:%ld waited for any terminated Process and got PID: %ld\n", curr_tid, check_pid);
  }
  else
  {
    bool is_zombie;
    is_zombie = pr_instance->checkProcessIsZombie(pid);
    
    if(is_zombie)
    {
      pr_instance->pid_waits_lock_.acquire();
      PidWaits* pid_waits;
      for (auto itr = pr_instance->pid_waits_.begin(); itr != pr_instance->pid_waits_.end(); itr++)
      {
        if(pid == (*itr)->getPid())
        {
          pid_waits = *itr;
          break;
        }
      }

      pid_waits->list_lock_.acquire();
      if (pid_waits->getNumPtids() == 0)
      {
        debug(WAIT_PID, "Thread:%ld found terminated Process PID: %ld\n", curr_tid, pid);
        delete_entry = true;
      }

      pid_waits->list_lock_.release();
      ProcessExitInfo pexit(pr_instance->getExitInfo(pid, delete_entry));
      pr_instance->pid_waits_lock_.release();

      if(status != NULL)
        *((int*)status) = (pexit.exit_val_&0xFF) + (1<<8);

      debug(WAIT_PID, "Thread:%ld found terminated Process PID: %ld\n", curr_tid, pid);
      pr_instance->unlockLists();
      return pexit.pid_;
    }

    bool is_used = pr_instance->checkProcessIsUsed(check_pid);  
    pr_instance->unlockLists();
    if(!is_used)
    {
      return -1;
    }
    pr_instance->pid_waits_lock_.acquire();
  }

  for (auto itr = pr_instance->pid_waits_.begin(); itr != pr_instance->pid_waits_.end(); itr++)
  {
    auto pid_waits = *itr;
    if (check_pid == pid_waits->getPid())
    {
      pr_instance->pid_waits_lock_.release();
      debug(WAIT_PID, "Thread: %ld waiting for termination of Process PID: %ld\n", curr_tid, check_pid);
      pid_waits->waitUntilReady(curr_pid, curr_tid);
      pr_instance->lockLists();
      pr_instance->pid_waits_lock_.acquire();
      pid_waits->list_lock_.acquire();
      pid_waits->removePtid(curr_pid, curr_tid);

      if(pid_waits->getNumPtids() == 0)
      {
        delete_entry = true;
        pid_waits->list_lock_.release();
        //debug(WAIT_PID, "Erase pid_waits_ element with pid: %ld\n", check_pid);
        //ProcessRegistry::instance()->pid_waits_.erase(itr);
        //delete pid_waits;
      }
      else
      {
        pid_waits->list_lock_.release();
      }
      
      ProcessExitInfo pexit(pr_instance->getExitInfo(check_pid, false));//delete_entry));
      
      if(status != NULL)
        *((int*)status) = (pexit.exit_val_&0xFF) + (1<<8);
      
      pr_instance->pid_waits_lock_.release();
      pr_instance->unlockLists();
      return pid;
    }
  }
  return -1;
}

size_t Syscall::getpid()
{
  return ((UserThread *)currentThread)->getProcess()->getPID();
}

size_t Syscall::setCancelState(size_t state, size_t oldstate)
{
  /// TODO MULTITHREADING: Param -1 (As it happens below as well)
  UserThread* thread = ((UserThread*)currentThread);
  if(state != thread->PTHREAD_CANCEL_ENABLE && state != thread->PTHREAD_CANCEL_DISABLE) return -1;
  if(oldstate != thread->cancelstate_ && oldstate != NULL) return -1;
  /// TODO MULTITHREADING: NonStd -2 (As it happens below as well)
  thread->cancelstate_ = state;
  return 0;
}

size_t Syscall::setCancelType(size_t type, size_t oldtype)
{
  UserThread* thread = ((UserThread*)currentThread);
  if(type != thread->PTHREAD_CANCEL_ASYNCHRONOUS && type != thread->PTHREAD_CANCEL_DEFERRED)
    return -1;
  if(oldtype != thread->canceltype_ && oldtype != NULL)
    return -1;
  thread->canceltype_ = type;
  return 0;
}