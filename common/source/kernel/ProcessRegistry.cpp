#include <mm/KernelMemoryManager.h>
#include "ProcessRegistry.h"
#include "Scheduler.h"
#include "kprintf.h"
#include "VfsSyscall.h"
#include "VirtualFileSystem.h"


ProcessRegistry* ProcessRegistry::instance_ = 0;

ProcessRegistry::ProcessRegistry(FileSystemInfo *root_fs_info, char const *progs[]) :
    Thread(root_fs_info, "ProcessRegistry", Thread::KERNEL_THREAD), progs_(progs), progs_running_(0),
    counter_lock_("ProcessRegistry::counter_lock_"),
    all_processes_killed_(&counter_lock_, "ProcessRegistry::all_processes_killed_"), list_lock_("ProcessRegistry::list_lock_")
{
  instance_ = this; // instance_ is static! -> Singleton-like behaviour
}

ProcessRegistry::~ProcessRegistry()
{
}

ProcessRegistry* ProcessRegistry::instance()
{
  return instance_;
}

void ProcessRegistry::Run()
{
  if (!progs_ || !progs_[0])
    return;

  debug(PROCESS_REG, "mounting userprog-partition \n");

  debug(PROCESS_REG, "mkdir /usr\n");
  assert( !VfsSyscall::mkdir("/usr", 0) );
  debug(PROCESS_REG, "mount idea1\n");
  assert( !VfsSyscall::mount("idea1", "/usr", "minixfs", 0) );

  debug(PROCESS_REG, "mkdir /dev\n");
  assert( !VfsSyscall::mkdir("/dev", 0) );
  debug(PROCESS_REG, "mount devicefs\n");
  assert( !VfsSyscall::mount(NULL, "/dev", "devicefs", 0) );


  KernelMemoryManager::instance()->startTracing();

  for (uint32 i = 0; progs_[i]; i++)
  {
    createProcess(progs_[i]);
  }

  counter_lock_.acquire();

  while (progs_running_)
    all_processes_killed_.wait();

  counter_lock_.release();

  debug(PROCESS_REG, "unmounting userprog-partition because all processes terminated \n");

  VfsSyscall::umount("/usr", 0);
  VfsSyscall::umount("/dev", 0);
  vfs.rootUmount();

  Scheduler::instance()->printStackTraces();

  Scheduler::instance()->printThreadList();

  kill();
}

void ProcessRegistry::processExit()
{
  counter_lock_.acquire();

  if (--progs_running_ == 0)
    all_processes_killed_.signal();

  counter_lock_.release();
}

void ProcessRegistry::processStart()
{
  counter_lock_.acquire();
  ++progs_running_;
  counter_lock_.release();
}

size_t ProcessRegistry::processCount()
{
  MutexLock lock(counter_lock_);
  return progs_running_;
}

void ProcessRegistry::createProcess(const char* path)
{
  debug(PROCESS_REG, "create process %s\n", path);
  UserProcess* process = new UserProcess(path, new FileSystemInfo(*working_dir_), getNewPID());
  debug(PROCESS_REG, "created userprocess %s\n", path);
  Scheduler::instance()->addNewThread(process->getThreads()->front());
  debug(PROCESS_REG, "added thread %s\n", path);
}

void ProcessRegistry::createProcess(UserProcess *process)
{
  ustl::string path = process->getFilename();
  debug(PROCESS_REG, "created forked userprocess %s\n", path.c_str());
  //Scheduler::instance()->addNewThread(process->getThreads()->front());
  debug(PROCESS_REG, "added thread %s\n", path.c_str());
}

size_t ProcessRegistry::getNewPID(){
  size_t new_pid = 0;

  list_lock_.acquire();
  // No released pid exists -> get a new pid from progs_running_
  if (unused_pids_.size() == 0)
  {
    counter_lock_.acquire();
    new_pid = progs_running_;
    counter_lock_.release();

    used_pids_.push_back(new_pid);
    list_lock_.release();

    debug(PROCESS_REG, "Added new PID %zu to the used PID list\n", new_pid);
    return new_pid;
  }

  // Read the first element in the unused_pids list and pop it
  new_pid = unused_pids_.front();
  unused_pids_.pop_front();
  // Add the popped pid to the used_pids list
  used_pids_.insert(used_pids_.end(), new_pid);
  list_lock_.release();

  debug(PROCESS_REG, "Moved PID %zu from the unused to the used PID list\n", new_pid);
  return new_pid;
}

void ProcessRegistry::releasePID(size_t pid){
  list_lock_.acquire();

  // Find the pid in the used_pids via an iterator (element deletion)
  auto itr = ustl::find(used_pids_.begin(), used_pids_.end(), pid);

  // Checks if the pid is occuring in the used list / valid
  assert(itr != used_pids_.end());

  // Delete the pid from the used_pids list
  used_pids_.erase(itr);
  
  // Insert the pid to the right position (ascending order)
  if (unused_pids_.size() != 0){
    for (auto itr = unused_pids_.begin(); itr != unused_pids_.end();itr++)
    { 
      if(pid > *itr)
      {
        unused_pids_.insert(itr+1, pid);
        break;
      }
      else 
      {
        unused_pids_.insert(itr, pid);
        break;
      }
    }
  } 
  else {
    // First element in the list
    unused_pids_.push_back(pid);
  }

  list_lock_.release();
  
  debug(PROCESS_REG, "Released PID %zu from the used PID list\n", pid);
}
