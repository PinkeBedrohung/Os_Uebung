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
#include "kmalloc.h"
#include "kstring.h"

UserProcess::UserProcess(ustl::string filename, FileSystemInfo *fs_info, uint32 pid, uint32 terminal_number) : 
        //holding_cow_(false),
        alive_lock_("UserProcess::alive_lock_"), threads_lock_("UserProcess::threads_lock_"), retvals_lock_("UserProcess::retvals_lock_"),
        fd_(VfsSyscall::open(filename, O_RDONLY)), pid_(pid), filename_(filename), fs_info_(fs_info), 
        terminal_number_(terminal_number)//, parent_process_(0), child_processes_()
        , num_threads_(0)
        

{
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process
   if (fd_ >= 0)
        loader_ = new Loader(fd_);

  if (!loader_ || !loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Error: loading %s failed!\n", filename.c_str());
    delete this;
    return;
  }

  //binary_fd_counter_ = new int32(1);
  UserThread *new_thread = new UserThread(this);
  new_thread->first_thread_ = true;
  addThread(new_thread);
}

UserProcess::UserProcess(UserProcess &process, UserThread *thread, int* retval) : //holding_cow_(true),
         alive_lock_("UserProcess::alive_lock_"), threads_lock_("UserProcess::threads_lock_"), retvals_lock_("UserProcess::retvals_lock_"),
         fd_(VfsSyscall::open(process.filename_, O_RDONLY)),//fd_(VfsSyscall::open(process.getFilename().c_str(), O_RDONLY))
         pid_(ProcessRegistry::instance()->getNewPID()),
         filename_(process.getFilename()), fs_info_(new FileSystemInfo(*process.getFsInfo())),
         terminal_number_(process.getTerminalNumber())//, parent_process_(&process), child_processes_()
         , num_threads_(0)
{
  ProcessRegistry::instance()->processStart();

  if(process.getLoader() && fd_ >= 0)
  {
    loader_ = new Loader(*process.getLoader(), fd_);
  }

  if (!loader_ || !loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Error: loading %s failed!\n", filename_.c_str());
    *retval = -1;
    return;
  }
  cpu_start_rdtsc = ArchThreads::rdtsc();
  //loader_ = new Loader(*process.getLoader(), fd_);

  debug(USERPROCESS, "Loader copy done\n");

  UserThread *new_thread = new UserThread(*thread, this);
  
  new_thread->user_registers_->rsp0 = (size_t)new_thread->getKernelStackStartPointer();
  new_thread->user_registers_->rax = 0;

  /*
  binary_fd_counter_ = process.binary_fd_counter_;
  (*binary_fd_counter_)++;
  */

  addThread(new_thread);
}

UserProcess::~UserProcess()
{
  debug(USERPROCESS, "~UserProcess - PID %zu\n", getPID());
  
  delete loader_;
  loader_ = 0;

  delete fs_info_;
  fs_info_ = 0;

  if (fd_ > 0)
  {
    /*
    (*binary_fd_counter_)--;

    if(*binary_fd_counter_ == 0)
    {
      */
      //delete binary_fd_counter_;
      VfsSyscall::close(fd_);
    //}
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

Thread* UserProcess::getThread(size_t tid)
{
  threads_lock_.acquire();
  for (auto thread : threads_)
  {
    if (thread->getTID() == tid)
    {
      threads_lock_.release();
      return thread;
    }
  }

  threads_lock_.release();
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

  if(((UserThread*)thread)->isStateJoinable())
    mapRetVals(thread->getTID(), (void*) ((UserThread*)thread)->retval_);

  alive_lock_.acquire();
  ((UserThread*)thread)->alive_cond_.broadcast();
  alive_lock_.release();

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
  /*
  fork_lock_->acquire();
  if (!cow_holding_ps)
  {
    fork_lock_->release();
    return 0;
  }
  */

  uint64 pml4 = ArchMemory::copyPagingStructure(loader_->arch_memory_.page_map_level_4_);
  loader_->arch_memory_.page_map_level_4_ = pml4;

  //ArchMemoryMapping m = currentThread->loader_->arch_memory_.resolveMapping(currentThread->kernel_registers_->rsp / PAGE_SIZE);
  //debug(USERPROCESS, "mapping: %zu\n", m.pml4_ppn);
  //debug(USERPROCESS, "TID: %zu\n", currentThread->getTID());
  //currentThread->kernel_registers_->cr3 = loader_->arch_memory_.page_map_level_4_ * PAGE_SIZE;
  //ArchThreads::atomic_set(currentThread->user_registers_->cr3, loader_->arch_memory_.page_map_level_4_ * PAGE_SIZE);
  

  ArchThreads::setAddressSpace(currentThread, loader_->arch_memory_);
  
  //ArchThreads::printThreadRegisters(currentThread);
  debug(USERPROCESS, "Copied pages and updated CR3 registers\n");
  /*
  cow_holding_ps->remove(this);
  if(cow_holding_ps->size() == 1)
  {
    cow_holding_ps->front()->cow_holding_ps = nullptr;
    delete cow_holding_ps;
    cow_holding_ps = nullptr;
  }
  fork_lock_->release();
  return pml4;
  */
  return 0;
}

void UserProcess::cancelNonCurrentThreads(Thread *thread)
{
  assert(thread);

  threads_lock_.acquire();
  for (auto it = threads_.begin(); it != threads_.end(); it++)
  {
     //debug(USERPROCESS, "You came to the wrong house fool \n");
    if ((*it)->getTID() != thread->getTID())
    {
      (*it)->kill();
      debug(USERPROCESS, "Removed TID %zu from Threadlist of PID %zu - %zu still assigned to the process\n", thread->getTID(), getPID(), getNumThreads());
    }
  }
  threads_lock_.release();
}

size_t UserProcess::getNumThreads()
{
  return num_threads_;
}

size_t UserProcess::createUserThread(size_t* tid, void* (*routine)(void*), void* args, void* entry_function)
{
  Thread* thread = new UserThread(this, routine, args, entry_function);
  threads_lock_.acquire();
  if(thread != NULL)
  {
    threads_.push_back(thread);
    num_threads_++;

    Scheduler::instance()->addNewThread(thread);
    *tid = thread->getTID();
    threads_lock_.release();
    return 0;
  }
  else
  {

    threads_lock_.release();
    return -1;
  }
}

void UserProcess::mapRetVals(size_t tid, void* retval)
{
  //TODO if detached don't store anything in mapretvals
  retvals_lock_.acquire();
  retvals_.insert(ustl::make_pair(tid, retval));
  retvals_lock_.release();
}

size_t UserProcess::cancelUserThread(size_t tid)
{
  threads_lock_.acquire();
  for (auto it = threads_.begin(); it != threads_.end(); it++)
  {
    if ((*it)->getTID() == tid)
    {
      debug(USERTHREAD, "Canceling thread: %ld\n",(*it)->getTID());
      
      ((UserThread*)it)->to_cancel_ = true;
      debug(USERTHREAD, "to_cancel_ set to: %d\n", (int)((UserThread*)it)->to_cancel_);

      debug(USERTHREAD, "switch_to_usersp: %d, is_currentthread: %d \n", ((UserThread*)it)->switch_to_userspace_, (int)((*it)==currentThread));
      if(((UserThread*)it)->switch_to_userspace_ && (*it) != currentThread)
      {
          threads_lock_.release();
        (*it)->kill();
      }
      else threads_lock_.release();
      return 0;
    }
  }

  threads_lock_.release();
  return -1;

}

int UserProcess::replaceProcessorImage(const char *path, char const *arg[])
{
  cancelNonCurrentThreads(currentThread);
  size_t return_val = 0;
  while(num_threads_ != 1)
  {Scheduler::instance()->yield();}

  if ((unsigned long long)*path >= USER_BREAK)
    {
      return_val = -1;
      return return_val;
    }

  //TODO: Old fd deleting has to be handled  
 
  
  int char_counter;
  size_t size_counter;
  int page_counter = 0;
  ustl::list<int> chars_per_arg;
  //currentThread;
  //debug(EXEC,"thread stack = %lx \n arg = %lx",current);
  //ArchMemoryMapping m = currentThread->loader_->arch_memory_.resolveMapping(currentThread->kernel_registers_->rsp / PAGE_SIZE);
 // m.pt[m.pti].present;
  size_t vpage_nr = USER_BREAK / PAGE_SIZE - (4 + MAX_STACK_ARG_PAGES) - (((UserThread*)currentThread)->getStackBaseNr()  * (MAX_STACK_PAGES + 1)) - ((UserThread*)currentThread)->getPageOffset() ;
  //(void *)(currentThreadRegisters->rbp * PAGE_SIZE + PAGE_SIZE - sizeof(pointer)
  /*debug(EXEC,"rsp = %p\n",(void*)currentThread->kernel_registers_->rsp );
  debug(EXEC,"rsp = %p\n",(void*)currentThread->kernel_registers_->rbp );
  debug(EXEC,"vpage_nr = %p \n" ,(void *)vpage_nr);
  debug(EXEC,"RECKEN DUATS OB %p\n",arg+73*sizeof(void*));
  debug(EXEC,"size of pointer %d",(int)sizeof(pointer));*/
  size_t Stackbegin = (vpage_nr * PAGE_SIZE + PAGE_SIZE - sizeof(pointer));
  debug(EXEC,"pageoffset = %d ", (int)((UserThread*)currentThread)->getPageOffset() );
  //Stackbegin = (vpage_nr * PAGE_SIZE );
  //Stackbegin = (vpage_nr * PAGE_SIZE + PAGE_SIZE + sizeof(pointer));
  debug(EXEC,"Stackbegin RSP = %p \n" ,(void *)Stackbegin);
  debug(EXEC,"Stackbegin RSP c8 = %p \n" ,(void *)(Stackbegin-48));
  debug(EXEC,"Stackbegin RSP d0= %p \n" ,(void *)(Stackbegin-40));
  debug(EXEC,"Stackbegin RSP d8 = %p \n" ,(void *)(Stackbegin-32));
  debug(EXEC,"Stackbegin RSP e0= %p \n" ,(void *)(Stackbegin-24));
  debug(EXEC,"Stackbegin RSP d0= %p \n" ,(void *)(Stackbegin-(sizeof(pointer) * 5)));

  //size_t lowerbound = Stackbegin  - sizeof(pointer) ; // - ((UserThread*)currentThread)->getPageOffset()//- PAGE_SIZE; // ;
  //debug(EXEC,"lowerbound = %p \n",(void*)lowerbound);
  debug(EXEC,"arg = %p \n",arg);
  // debug(EXEC,"arg rechnung = %p \n",(void*)((((size_t)arg)-PAGE_SIZE + sizeof(pointer))/PAGE_SIZE));
  //debug(EXEC,"arg = %p \n",arg+sizeof(void*));
  size_t test = sizeof(pointer);
 //debug(EXEC," WENNS DES IS FICK I WEN = %p \n", (void*)test );
  if(arg != NULL)
  {
    size_counter = sizeof(char*);           //  NULL pointer at end of args
    size_t arg_index = 0;
    for (arg_index = 0; arg[arg_index] != NULL; arg_index++)
    {
     // debug(EXEC,"sollte gestoppt haben \n");
      size_counter += sizeof(char*);
      char_counter = 1;
      for (size_t pi = 0; arg[arg_index][pi] != '\0'; pi++)
      {
        char_counter++;
        size_counter += sizeof(char);
      }
      size_counter += sizeof(char);
     // debug(EXEC, "char_counter: %d\n", (int)char_counter);
      chars_per_arg.insert(chars_per_arg.end(), char_counter);
      //debug(EXEC,"arg_index = %d \n", (int)arg_index);
      if(arg_index % 2 == 0 && (arg + arg_index) <= (void*)(Stackbegin-(sizeof(pointer) * 5)) && (arg + arg_index) > (void*)(Stackbegin - sizeof(pointer) * 6 )  )
      {
        //debug(EXEC,"I hoss mei lebn\n");
      }
      if( (arg + arg_index) <= (void*)(Stackbegin-(sizeof(pointer) * 3)) && (arg + arg_index) > (void*)(Stackbegin - sizeof(pointer) * 4 )  )
      {
       // debug(EXEC,"I hoss mei lebn gerade \n");
      }
    /*  if((arg + arg_index) < (void *)(Stackbegin-32) &&  (arg + arg_index) >= (void *)(Stackbegin-40))arg_index % 2 != 0 &&
      {
        debug(EXEC,"I hoss mei lebn\n");
      }*/
      debug(EXEC,"----------------------------------------------------------------------\n");
      debug(EXEC,"arg[%d] = %p \n",(int)arg_index,((void*)((size_t*)((size_t)arg[arg_index]))));
      
      //debug(EXEC,"arg value guess = %p \n",((void*)((size_t*)((size_t)((size_t)arg - (arg_index+1) * sizeof(void*) * MAX_STACK_ARG_PAGES * PAGE_SIZE)))));
      debug(EXEC,"*arg + arg_index = %p\n",(arg + arg_index));

      debug(EXEC,"*arg = %p\n",*arg);
      
    }
    debug(EXEC,"----------------------------------------------------------------------\n");
    debug(EXEC,"*arg + arg_index = %p\n",(arg + arg_index + 1));
    arg_index++;
    //debug(EXEC,"*arg + arg_index = %p\n",(arg + arg_index + 1));
    //debug(EXEC,"size_counter = %d\n", (int)size_counter);
    //debug(EXEC,"MAX ARG SIZE = %d \n", ARG)
    page_counter = size_counter / PAGE_SIZE;

    if ((size_counter % PAGE_SIZE) != 0)
    {
      page_counter++;
    }
    assert(page_counter != 0 && "Allocated 0 pages for arguments of exec");
  }
  if ((page_counter) > MAX_STACK_ARG_PAGES)
  {
    return_val = -1;
    return return_val;
  }
  //debug(EXEC,"UserProcess  page counter = %d\n",page_counter);

  filename_ = ustl::string(path);
  int32_t fd = VfsSyscall::open(filename_.c_str(), O_RDONLY); 
  debug(USERPROCESS,"Filedescriptor ========= %d \n",fd);
  //currentThread->user_registers_ = new ArchThreadRegisters();
  //
  Loader* loader = new Loader(fd);
  loader->loadExecutableAndInitProcess();

  ArchThreads::printThreadRegisters(currentThread);


  char **argv = (char**)kmalloc(((size_t)chars_per_arg.size() + 1) * sizeof(char*));

  int argv_size = 1;
  for (int arg_index = 0; arg_index < (int)chars_per_arg.size(); arg_index++)
  {
    argv_size++;
    char *arg_p = (char *)kmalloc(((size_t)chars_per_arg.at(arg_index)) * sizeof(char));
    memcpy((void*)arg_p, (void *)(size_t)arg[arg_index], (size_t)chars_per_arg.at(arg_index));
    argv[arg_index] = arg_p;
  }

  argv[(size_t)chars_per_arg.size()] = NULL;

  Loader *old_loader = loader_;
  ssize_t old_fd = fd_;
  loader_ = loader;
  fd_=fd;
  currentThread->loader_ = loader;
  
  //ArchThreads::setAddressSpace(currentThread, loader_->arch_memory_);
  return_val = ((UserThread*)currentThread)->execStackSetup(argv, chars_per_arg, page_counter, size_counter);
  if(return_val != 0)
  {
    return return_val;
  }
  delete old_loader;
  VfsSyscall::close(old_fd);

  for (size_t i = 0; argv[i] != NULL; i++)
  {
    kfree(argv[i]);
  }
  kfree(argv);

  while (chars_per_arg.size() != 0)
  {
      chars_per_arg.pop_front();
  }

  ArchThreads::printThreadRegisters(currentThread);

  currentThread->switch_to_userspace_ = 1;
  Scheduler::instance()->yield();
  return_val = 0;
  return return_val;
}
  

