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
  if(((UserThread*)currentThread)->to_cancel_)
  {
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
    case sc_clock:
      return_value = clock();
      break;
    case sc_sleep:
      return_value = sleep((unsigned int) arg1);
      break;
    case sc_pseudols:
      VfsSyscall::readdir((const char*) arg1);
      break;
    case sc_fork:
      return_value = fork();
      break;
    case  sc_exec:
      return_value = exec((const char *)arg1, (char const**)arg2);
      break;
    default:
      kprintf("Syscall::syscall_exception: Unimplemented Syscall Number %zd\n", syscall_number);
  }
  return return_value;
}

void Syscall::exit(size_t exit_code)
{
  debug(SYSCALL, "Syscall::EXIT: called, exit_code: %zd\n", exit_code);
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
  if((size_t)start_routine >= USER_BREAK || (size_t)arg >= USER_BREAK) return -1U;
  if((size_t)thread < USER_BREAK || (size_t)attr < USER_BREAK || (size_t)start_routine < USER_BREAK)
  {
    UserProcess* uprocess = ((UserThread*)currentThread)->getProcess();
    return uprocess->createUserThread((size_t*) thread, (void* (*)(void*))start_routine, (void*) arg, (void*) entry_function);
  }

    return -1U;
  
}

void Syscall::exitThread(size_t retval)
{
  //TODO: when a thread is cancelled, store -1 in retval so join can also return -1 as PTHREAD_CANCELED
  
    UserProcess* process = ((UserThread*)currentThread)->getProcess();
    process->mapRetVals(currentThread->getTID(), (void*) retval);
  
  currentThread->kill();
}

size_t Syscall::clock()
{
  unsigned long long rdtsc = ArchThreads::rdtsc(); //now
  unsigned long long difference = rdtsc -((UserThread*)currentThread)->getProcess()->cpu_start_rdtsc;
  //debug(SYSCALL,"Difference: %lld\n", difference);
  //debug(SYSCALL,"rdtsc: %lld\n", rdtsc);
  size_t retval = (difference)/(Scheduler::instance()->average_rdtsc_/(54925439/1000));
  //debug(SYSCALL,"retval: %ld\n", retval);
  return (size_t) retval; //clock ticks
  //The value returned is expressed in clock ticks.
}

size_t Syscall::sleep(unsigned int seconds)
{
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
  if ((size_t)*path >= USER_BREAK)
  {
    return -1;
  }

  if(((UserThread*)currentThread)->getProcess() == 0)
  {
    debug(MAIN,"jow du startest grod mit da shell a exec is ned so nice\n");
    return -1;
  }

  return ((UserThread *)currentThread)->getProcess()->replaceProcessorImage(path, arg);

  
  
  
  
}

size_t Syscall::joinThread(size_t thread, void** value_ptr)
{
  
  if(thread == NULL)
  {
    //debug(SYSCALL, "Thread to be joined is non-existent!\n");
    return (size_t)-1U;
  }


  // TODO
  // check if the thread was canceled and pass the PTHREAD_CANCELED to value_ptr
  // SOLVED: exit will pass -1 as retval when cancel is called

  UserThread* calling_thread = ((UserThread*)currentThread);
  UserProcess* current_process = ((UserThread*)currentThread)->getProcess(); 
  UserThread* thread_to_join = ((UserThread*)current_process->getThread(thread));

  if(thread == calling_thread->getTID())
  {
    //debug(SYSCALL, "Thread is calling join on itselt")
    return (size_t)-1U;
  }

  current_process->retvals_lock_.acquire();
  if(current_process->retvals_.find(thread) == current_process->retvals_.end())
  {
    current_process->retvals_lock_.release();

    //TODO check if other thread in join chain of thread_to_join is waiting for our calling_thread to avoid join deadlock
    if(calling_thread->chainJoin(thread_to_join->getTID()))
    {
      return (size_t) -1U;
    }
  
    //current_process->threads_lock_.acquire();
    calling_thread->join_ = thread_to_join;
    current_process->alive_lock_.acquire();
    //current_process->threads_lock_.release();
    thread_to_join->alive_cond_.waitAndRelease();
    calling_thread->join_ = NULL;
  }

  if(current_process->retvals_lock_.isHeldBy(currentThread))
  {
    current_process->retvals_lock_.release();
  }
  

  if(value_ptr != NULL)
  {
    if((uint64)value_ptr <= USER_BREAK && 
       current_process->retvals_.find(thread) != current_process->retvals_.end())
      *value_ptr = current_process->retvals_.at(thread);
    else
      return (size_t) -1U;
  }

  return (size_t) 0;
}

size_t Syscall::cancelThread(size_t tid)
{
  UserProcess* process = ((UserThread*)currentThread)->getProcess();
  process->mapRetVals(tid, (void*) -1);
  debug(SYSCALL, "Calling cancelUserThread on Thread: %ld\n", tid);
  return process->cancelUserThread(tid);
}