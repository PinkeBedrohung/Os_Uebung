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
        fd_(VfsSyscall::open(filename, O_RDONLY)), pid_(pid), filename_(filename), fs_info_(fs_info), 
        terminal_number_(terminal_number), threads_lock_("UserProcess::threads_lock_"), num_threads_(0)
{
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process
  
  if (fd_ >= 0)
      loader_ = new Loader(fd_);

  if (!loader_ || !loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Error: loading %s failed!\n", filename.c_str());
    ProcessRegistry::instance()->processExit();
    ProcessRegistry::instance()->releasePID(pid);
    return;
  }

  addThread(new UserThread(this));
}

UserProcess::UserProcess(UserProcess &process, UserThread *thread) : fd_(process.getFd()), pid_(ProcessRegistry::instance()->getNewPID()),
         filename_(process.getFilename()), loader_(process.getLoader()), fs_info_(new FileSystemInfo(*process.getFsInfo())),
         terminal_number_(process.getTerminalNumber()), threads_lock_("UserProcess::threads_lock_"), num_threads_(0)
{
  ProcessRegistry::instance()->processStart();

  loader_ = new Loader(fd_);
  
  if (!loader_ || !loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Error: loading %s failed!\n", filename_.c_str());
    ProcessRegistry::instance()->processExit();
    ProcessRegistry::instance()->releasePID(pid_);
    return;
  }

  UserThread *new_thread = new UserThread(*thread, this);

  addThread(new_thread);
  Scheduler::instance()->addNewThread(new_thread);

  loader_->arch_memory_.page_map_level_4_ = process.getLoader()->arch_memory_.page_map_level_4_;
  
  memcpy(new_thread->kernel_stack_ + 1, thread->kernel_stack_ + 1, (sizeof(new_thread->kernel_stack_)-2)/sizeof(uint32));
  memcpy(new_thread->kernel_registers_, thread->kernel_registers_, sizeof(ArchThreadRegisters));
  memcpy(new_thread->user_registers_, thread->user_registers_, sizeof(ArchThreadRegisters));
  
  //ArchMemoryMapping m = loader_->arch_memory_.resolveMapping(0);
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

void UserProcess::setPID(size_t pid){
  pid_ = pid;
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

void UserProcess::addThread(Thread *thread){
  assert(thread);
  
  threads_lock_.acquire();
  threads_.push_back(thread);
  num_threads_++;
  threads_lock_.release();

  //debug(USERPROCESS, "Added thread with TID %zu to process with PID %zu\n", thread->getTID(), getPID());
}

void UserProcess::removeThread(Thread *thread){
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

void UserProcess::copyPages()
{
  uint64 pml4 = ArchMemory::copyPagingStructure(loader_->arch_memory_.page_map_level_4_);
  loader_->arch_memory_.page_map_level_4_ = pml4;

  for (auto thread : threads_)
  {
    thread->user_registers_->cr3 = pml4 * PAGE_SIZE;
    thread->kernel_registers_->cr3 = pml4 * PAGE_SIZE;
  }
}

size_t UserProcess::getNumThreads()
{
  return num_threads_;
}