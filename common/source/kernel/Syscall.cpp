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
    case sc_pthread_exit:
      exitThread(arg1);
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
  if (!((UserThread*)currentThread)->getProcess()->cow_holding_ps)
  {
    ((UserThread *)currentThread)->getProcess()->cow_holding_ps = new ustl::list<UserProcess *>();
  }
  
  if(!((UserThread*)currentThread)->getProcess()->fork_lock_)
  {
    ((UserThread *)currentThread)->getProcess()->fork_lock_ = new Mutex("UserProcess::cow_lock_");
  }  
    

  MutexLock lock(*((UserThread *)currentThread)->getProcess()->fork_lock_);
  ((UserThread *)currentThread)->getProcess()->cow_holding_ps->push_back(((UserThread *)currentThread)->getProcess());
  
  ArchMemory::writeable(((UserThread *)currentThread)->getProcess()->getLoader()->arch_memory_.page_map_level_4_, 0);

  UserProcess *new_process = new UserProcess(*((UserThread *)currentThread)->getProcess(), (UserThread*)currentThread);
  ProcessRegistry::instance()->createProcess(new_process);

  return new_process->getPID();
}
size_t Syscall::createThread(size_t thread, size_t attr, size_t start_routine, size_t arg, size_t entry_function)
{
  if((size_t)start_routine >= USER_BREAK || (size_t)arg >= USER_BREAK) return -1U;
  if((size_t)thread < USER_BREAK || (size_t)attr < USER_BREAK || (size_t)start_routine < USER_BREAK)
  {
    UserProcess* uprocess = ((UserThread*)currentThread)->getProcess();
    return uprocess->createUserThread((size_t*) thread, (void* (*)(void*))start_routine, (void*) arg, (void*) entry_function);
  }
  else
  {
    exit(50);
    return -1U;
  }
}

void Syscall::exitThread(size_t retval)
{
  // TODO: Add retval to process for join
  if (retval) {
    UserProcess* process = ((UserThread*)currentThread)->getProcess();
    process->mapRetVals(currentThread->getTID(), (void*) retval);
  }
  // TODO: End
  currentThread->kill();
}

size_t Syscall::clock()
{
  unsigned long long rdtsc = ArchThreads::rdtsc(); //now
  unsigned long long difference = rdtsc - currentThread->cpu_start_rdtsc;
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
