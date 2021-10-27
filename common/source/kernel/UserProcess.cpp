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
        //holding_cow_(false),
        fd_(VfsSyscall::open(filename, O_RDONLY)), pid_(pid), filename_(filename), fs_info_(fs_info), 
        terminal_number_(terminal_number), parent_process_(0), child_processes_(), threads_lock_("UserProcess::threads_lock_"), num_threads_(0)
{
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process
  created_threads_ = 0;
   if (fd_ >= 0)
        loader_ = new Loader(fd_);

  if (!loader_ || !loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Error: loading %s failed!\n", filename.c_str());
    ProcessRegistry::instance()->processExit();
    ProcessRegistry::instance()->releasePID(pid);
    return;
  }

  binary_fd_counter_ = new int32(1);

  addThread(new UserThread(this));
}

UserProcess::UserProcess(UserProcess &process, UserThread *thread) : //holding_cow_(true),
         fd_(process.getFd())//fd_(VfsSyscall::open(process.getFilename().c_str(), O_RDONLY))
        , pid_(ProcessRegistry::instance()->getNewPID()),
         filename_(process.getFilename()), fs_info_(new FileSystemInfo(*process.getFsInfo())),
         terminal_number_(process.getTerminalNumber()),parent_process_(&process), child_processes_(), threads_lock_("UserProcess::threads_lock_"), num_threads_(0)
{
  ProcessRegistry::instance()->processStart();

  loader_ = new Loader(*process.getLoader(), fd_);

  debug(USERPROCESS, "Loader copy done\n");

  UserThread *new_thread = new UserThread(*thread, this);
  
  new_thread->user_registers_->rsp0 = (size_t)new_thread->getKernelStackStartPointer();
  new_thread->user_registers_->rax = 0;

  //ArchThreads::printThreadRegisters(thread);
  //ArchThreads::printThreadRegisters(new_thread);
  cow_holding_ps = process.cow_holding_ps;
  cow_holding_ps->push_back(this);
  process.child_processes_.push_back(this);
  fork_lock_ = parent_process_->fork_lock_;

  binary_fd_counter_ = process.binary_fd_counter_;
  (*binary_fd_counter_)++;

  addThread(new_thread);
}

UserProcess::~UserProcess()
{
  debug(USERPROCESS, "~UserProcess - PID %zu\n", getPID());

  bool last_fork_lock_holder = false;
  if (fork_lock_)
  {
    fork_lock_->acquire();
    if(cow_holding_ps && cow_holding_ps->size() == 2)
    {
      ArchMemory::writeable(loader_->arch_memory_.page_map_level_4_, 1, Decrement);

      last_fork_lock_holder = true;
      cow_holding_ps->remove(this);
      cow_holding_ps->front()->cow_holding_ps = nullptr;
      cow_holding_ps->front()->fork_lock_ = nullptr;
      delete cow_holding_ps;
      cow_holding_ps = nullptr;
      
    }
    else if(cow_holding_ps && cow_holding_ps->size() > 2)
    {
      ArchMemory::writeable(loader_->arch_memory_.page_map_level_4_, -1, Decrement);
      cow_holding_ps->remove(this);
    }
    
    fork_lock_->release();
  }

  if(last_fork_lock_holder && !fork_lock_->heldBy())
    delete fork_lock_;

  fork_lock_ = nullptr;
  
  // Remove the current process from the child processes list of the parent
  if(parent_process_)
  {
    for (auto &parents_child_process : parent_process_->child_processes_)
    {
      if (parents_child_process == this)
      {
        parent_process_->child_processes_.remove(this);
      }
    }
  }
    // Set the parent of the children to the parent process
    for (auto &child_process : child_processes_)
    {
      child_process->parent_process_ = parent_process_;
    }
  
  delete loader_;
  loader_ = 0;
  
  if (fd_ > 0)
  {
    (*binary_fd_counter_)--;

    if(*binary_fd_counter_ == 0)
    {
      delete binary_fd_counter_;
      VfsSyscall::close(fd_);
    }
  }
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
  created_threads_++;
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

uint64 UserProcess::copyPages()
{
  //ArchMemoryMapping d = currentThread->loader_->arch_memory_.resolveMapping(currentThread->kernel_registers_->rsp / PAGE_SIZE);
  //debug(USERPROCESS, "accesscounter: %zu\n", d.pml4->access_ctr);
  uint64 pml4 = ArchMemory::copyPagingStructure(loader_->arch_memory_.page_map_level_4_);
  //debug(USERPROCESS, "pml4: %zu or_pml4: %zu\n", pml4, loader_->arch_memory_.page_map_level_4_);
  //ArchMemoryMapping m = currentThread->loader_->arch_memory_.resolveMapping(currentThread->kernel_registers_->rsp / PAGE_SIZE);
  //debug(USERPROCESS, "accesscounter: %zu\n", m.pml4->access_ctr);
  //ArchThreads::printThreadRegisters(currentThread);
  loader_->arch_memory_.page_map_level_4_ = pml4;

  //ArchMemoryMapping m = currentThread->loader_->arch_memory_.resolveMapping(currentThread->kernel_registers_->rsp / PAGE_SIZE);
  //debug(USERPROCESS, "mapping: %zu\n", m.pml4_ppn);
  //debug(USERPROCESS, "TID: %zu\n", currentThread->getTID());
  //currentThread->kernel_registers_->cr3 = loader_->arch_memory_.page_map_level_4_ * PAGE_SIZE;
  //ArchThreads::atomic_set(currentThread->user_registers_->cr3, loader_->arch_memory_.page_map_level_4_ * PAGE_SIZE);
  ArchThreads::setAddressSpace(currentThread, loader_->arch_memory_);
  //ArchThreads::printThreadRegisters(currentThread);
  debug(USERPROCESS, "Copied pages and updated CR3 registers\n");

  cow_holding_ps->remove(this);
  if(cow_holding_ps->size() == 1)
  {
    cow_holding_ps->front()->cow_holding_ps = nullptr;
    cow_holding_ps->front()->fork_lock_ = nullptr;
    delete cow_holding_ps;
    cow_holding_ps = nullptr;
  }
  
  return pml4;
}

size_t UserProcess::getNumThreads()
{
  return num_threads_;
}

size_t UserProcess::createUserThread(size_t* tid, void* (*routine)(void*), void* args, void* entry_function)
{
  Thread* thread = new UserThread(this, routine, args, entry_function);
  if(thread != NULL)
  {
  addThread(thread);

  Scheduler::instance()->addNewThread(thread);
  *tid = thread->getTID();
  return 0;
  }
  else{
    return -1;
  }
}

void UserProcess::mapRetVals(size_t tid, void* retval)
{
  for (auto it = threads_.begin(); it != threads_.end(); it++)
  {
    if(((UserThread*)it)->getTID() == tid)
    {
      retvals_.insert(ustl::make_pair(tid, retval));
    }
  }
}