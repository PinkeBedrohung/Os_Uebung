#include <mm/KernelMemoryManager.h>
#include "ProcessRegistry.h"
#include "Scheduler.h"
#include "UserProcess.h"
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
  UserProcess* process = new UserProcess(path, new FileSystemInfo(*working_dir_), getUnusedPID());
  debug(PROCESS_REG, "created userprocess %s\n", path);
  Scheduler::instance()->addNewThread(process->getThread());
  debug(PROCESS_REG, "added thread %s\n", path);
}

uint32 ProcessRegistry::getUnusedPID(){
  uint32 new_pid = 0;

  list_lock_.acquire();
  if (unused_pids.size() == 0)
  {
    counter_lock_.acquire();
    new_pid = progs_running_;
    counter_lock_.release();

    used_pids.push_back(new_pid);
    list_lock_.release();
    debug(PROCESS_REG, "Added new PID %u to the used PID list\n", new_pid);
    return new_pid;
  }

  new_pid = unused_pids.front();
  unused_pids.pop_front();
  list_lock_.release();

  debug(PROCESS_REG, "Moved PID %u from the unused to the used PID list\n", new_pid);
  return new_pid;
}

void ProcessRegistry::releasePID(uint32 pid){
  list_lock_.acquire();

  size_t pid_index = (size_t)-1;
  for (pid_index = 0; pid_index < used_pids.size(); pid_index++)
  {
    if(used_pids.at(pid_index) == pid)
      break;
  }
  assert(pid_index != (size_t)-1);

  auto pid_itr = ustl::find(used_pids.begin(), used_pids.end(), pid);

  if(pid_itr != used_pids.end()){
    used_pids.erase(pid_itr);
  }
  unused_pids.push_back(pid);

  list_lock_.release();

  debug(PROCESS_REG, "Released PID %u from the used PID list\n", pid);
}
