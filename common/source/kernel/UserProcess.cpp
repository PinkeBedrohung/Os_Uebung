#include "ProcessRegistry.h"
#include "UserProcess.h"
#include "kprintf.h"
#include "Console.h"
#include "Loader.h"
#include "VfsSyscall.h"
#include "File.h"
#include "ArchMemory.h"
#include "PageManager.h"
#include "ArchThreads.h"
#include "offsets.h"
#include "UserThread.h"

UserProcess::UserProcess(ustl::string filename, FileSystemInfo *fs_info, uint32 pid, uint32 terminal_number) : 
        alive_lock_("UserProcess::alive_lock_"),
        threads_lock_("UserProcess::threads_lock_"),
        fd_(VfsSyscall::open(filename, O_RDONLY)), pid_(pid), filename_(filename), fs_info_(fs_info),
        terminal_number_(terminal_number), num_threads_(0)
{
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process
   if (fd_ >= 0)
        loader_ = new Loader(fd_);

    if (!loader_ || !loader_->loadExecutableAndInitProcess())
    {
        debug(USERPROCESS, "Error: loading %s failed!\n", filename.c_str());
        loader_ = 0;
        //kill();
        return;
    }
  
  
  add_thread(new UserThread(this));
}

UserProcess::~UserProcess()
{
  debug(USERPROCESS, "~UserProcess - PID %zu\n", getPID());

  delete loader_;
  loader_ = 0;

  if (fd_ > 0)
      VfsSyscall::close(fd_);

  ProcessRegistry::instance()->releasePID(pid_);
  ProcessRegistry::instance()->processExit();
}

// This function needs to be changed or substituted with other methods
UserProcess::ThreadList* UserProcess::getThreads(){
  return &threads_;
}

Thread* UserProcess::getThread(size_t tid)
{
  for (auto thread : threads_)
  {
    if (thread->getTID() == tid)
    {
      return thread;
    }
  }

  return NULL;
}

size_t UserProcess::getPID(){
  return pid_;
}

ustl::string UserProcess::getFilename(){
  return filename_;
}

Loader* UserProcess::getLoader(){
  return loader_;
}

uint32 UserProcess::getTerminalNumber(){
  return terminal_number_;
}

FileSystemInfo* UserProcess::getFsInfo(){
  return fs_info_;
}

int32 UserProcess::getFd(){
  return fd_;
}

void UserProcess::add_thread(Thread *thread) {
  assert(thread);
  
  threads_lock_.acquire();
  threads_.push_back(thread);
  num_threads_++;
  threads_lock_.release();

  debug(USERPROCESS, "Added thread with TID %zu to process with PID %zu\n", thread->getTID(), getPID());
}

void UserProcess::remove_thread(Thread *thread){
  assert(thread);

  threads_lock_.acquire();
  for (auto it = threads_.begin(); it != threads_.end(); it++)
  {
    if ((*it)->getTID() == thread->getTID())
    {
      threads_.erase(it);
      num_threads_--;
      debug(USERPROCESS, "Removed TID %zu from Threadlist of PID %zu - %zu still assigned to the process\n", thread->getTID(), getPID(), getNumThreads());
      break;
    }
  }
  threads_lock_.release();
}


size_t UserProcess::getNumThreads(){
  return num_threads_;
}

size_t UserProcess::createUserThread(size_t* tid, void* (*routine)(void*), void* args, void* entry_function)
{
  Thread* thread = new UserThread(this, tid, routine, args, entry_function);
  add_thread(thread);

  Scheduler::instance()->addNewThread(thread);
  return 0;
}

void UserProcess::mapRetVals(size_t tid, void* retval)
{
  // TODO: Add retvals lock
  retvals_.insert(ustl::make_pair(tid, retval));
}
