#include <mm/KernelMemoryManager.h>
#include "ProcessRegistry.h"
#include "Scheduler.h"
#include "kprintf.h"
#include "VfsSyscall.h"
#include "VirtualFileSystem.h"


ProcessRegistry* ProcessRegistry::instance_ = 0;

ProcessRegistry::ProcessRegistry(FileSystemInfo *root_fs_info, char const *progs[]) :
    Thread(root_fs_info, "ProcessRegistry", Thread::KERNEL_THREAD), pid_waits_lock_("ProcessRegistry::pid_waits_lock_"),
    pid_available_lock_("ProcessRegistry::pid_availabe_lock_"), pid_available_cond_(&pid_available_lock_,"ProcessRegistry::pid_waits_cond_"),
    progs_(progs), progs_running_(0), counter_lock_("ProcessRegistry::counter_lock_"),
    all_processes_killed_(&counter_lock_, "ProcessRegistry::all_processes_killed_"), list_lock_("ProcessRegistry::list_lock_"), 
    exit_info_lock_("ProcessRegistry::exit_info_lock_"), progs_started_(0)
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
  Scheduler::instance()->addNewThread(process->getThreads()->front());
  // debug(PROCESS_REG, "added thread %s\n", path.c_str());
}

size_t ProcessRegistry::getNewPID(){
  size_t new_pid = 0;

  list_lock_.acquire();
  // No released pid exists -> get a new pid from progs_started_
  if (unused_pids_.size() == 0)
  {
    counter_lock_.acquire();
    new_pid = progs_started_++;
    counter_lock_.release();

    used_pids_.push_back(new_pid);

    PidWaits *new_pid_waits = new PidWaits(new_pid);
    pid_waits_lock_.acquire();
    pid_waits_.push_back(new_pid_waits);
    pid_waits_lock_.release();

    list_lock_.release();

    debug(PROCESS_REG, "Added new PID %zu to the used PID list\n", new_pid);
    return new_pid;
  }

  // Read the first element in the unused_pids list and pop it
  new_pid = unused_pids_.front();
  unused_pids_.pop_front();
  // Add the popped pid to the used_pids list
  used_pids_.insert(used_pids_.end(), new_pid);

  PidWaits *new_pid_waits = new PidWaits(new_pid);
  pid_waits_lock_.acquire();
  pid_waits_.push_back(new_pid_waits);
  pid_waits_lock_.release();

  list_lock_.release();

  debug(PROCESS_REG, "Moved PID %zu from the unused to the used PID list\n", new_pid);
  return new_pid;
}

void ProcessRegistry::releasePID(size_t pid){
  assert(list_lock_.isHeldBy(currentThread));

  // Find the pid in the zobmie_pids via an iterator (element deletion)
  auto itr = ustl::find(zombie_pids_.begin(), zombie_pids_.end(), pid);

  // Checks if the pid is occuring in the zombie_pids list / valid
  debug(WAIT_PID, "Delete PID %ld from zombie_pids with size %ld\n", *itr, zombie_pids_.size());
  assert(itr != zombie_pids_.end());

  // Delete the pid from the zombie_pids list
  zombie_pids_.erase(itr);
  
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
  
  debug(PROCESS_REG, "Released PID %zu from the zombie PID list\n", pid);
}

bool ProcessRegistry::checkProcessIsUsed(size_t pid)
{
  assert(list_lock_.isHeldBy(currentThread));

  for (auto &used_pid : used_pids_)
  {
    if(pid == used_pid)
    {
      return true;
    }
  }
  return false;
}

bool ProcessRegistry::checkProcessIsZombie(size_t pid)
{
  assert(list_lock_.isHeldBy(currentThread));

  for (auto &zombie_pid : zombie_pids_)
  {
    if(pid == zombie_pid)
    {
      return true;
    }
  }
  return false;
}

void ProcessRegistry::lockLists()
{
  list_lock_.acquire();
}

void ProcessRegistry::unlockLists()
{
  list_lock_.release();
}

void ProcessRegistry::addExitInfo(ProcessExitInfo &pexit_info)
{
  exit_info_lock_.acquire();
  debug(WAIT_PID, "addExitInfo: PID %ld with exit_code %ld\n", pexit_info.pid_, pexit_info.exit_val_);
  pexit_infos_.push_back(pexit_info);
  exit_info_lock_.release();
}

ProcessExitInfo ProcessRegistry::getExitInfo(size_t pid, bool delete_entry)
{
  assert(list_lock_.isHeldBy(currentThread));
  MutexLock lock(exit_info_lock_);

  for (ustl::list<ProcessExitInfo>::iterator itr = pexit_infos_.begin(); itr != pexit_infos_.end(); itr++)
  {
    if(pid == (*itr).pid_)
    {
      debug(WAIT_PID, "getExitInfo: found PID %ld\n", pid);
      ProcessExitInfo ret(*itr);
      if(delete_entry)
      {
        assert(pid_waits_lock_.isHeldBy(currentThread));
        
        for (auto itr = pid_waits_.begin(); itr != pid_waits_.end(); itr++)
        {
          if(pid == (*(*itr)).getPid())
          {
            debug(WAIT_PID, "getExitInfo: delete PID %ld from PidWaits\n", pid);
            delete *itr;
            pid_waits_.erase(itr);
            break;
          }
        }
        assert(pid_waits_lock_.isHeldBy(currentThread));

        pexit_infos_.erase(itr);

        debug(WAIT_PID, "Release PID %ld from zombie PIDs List\n", pid);
        releasePID(pid);
        debug(WAIT_PID, "Release PID %ld from zombie PIDs List\n", pid);
      }
      return ret;
    }
  }

  assert(0 && "The pid exit infos are not present. Some checks are wrong!");
  return ProcessExitInfo(-1, -1);
}

void ProcessRegistry::makeZombiePID(size_t pid)
{
  assert(list_lock_.isHeldBy(currentThread));
  
  // Find the pid in the used_pids via an iterator (element deletion)
  auto itr = ustl::find(used_pids_.begin(), used_pids_.end(), pid);
  debug(WAIT_PID, "Delete PID %ld from used Pids List with size %ld\n", *itr, used_pids_.size());
  // Checks if the pid is occuring in the used list / valid
  assert(itr != used_pids_.end());

  // Delete the pid from the used_pids list
  used_pids_.erase(itr);
  
  // Insert the pid to the right position (ascending order)
  if (zombie_pids_.size() != 0){
    for (auto itr = zombie_pids_.begin(); itr != zombie_pids_.end();itr++)
    { 
      if(pid > *itr)
      {
        zombie_pids_.insert(itr+1, pid);
        break;
      }
      else 
      {
        zombie_pids_.insert(itr, pid);
        break;
      }
    }
  } 
  else {
    // First element in the list
    zombie_pids_.push_back(pid);
  }
  debug(WAIT_PID, "Zombie PID list is now %ld element big\n", zombie_pids_.size());
  pid_waits_lock_.acquire();
  for (auto itr = pid_waits_.begin(); itr != pid_waits_.end(); itr++)
  {
    if(pid == (*(*itr)).getPid())
    {
      (*(*itr)).signalReady();
      break;
    }
  }
  pid_waits_lock_.release();
  
  debug(PROCESS_REG, "Made PID %zu a zombie\n", pid);
}

size_t ProcessRegistry::getFirstZombiePID()
{
  assert(list_lock_.isHeldBy(currentThread));

  if(zombie_pids_.size() == 0)
    return (size_t)-1UL;

  size_t pid = zombie_pids_.at(0);
  return pid;
}

size_t ProcessRegistry::getUsedPid()
{
  assert(list_lock_.isHeldBy(currentThread));

  for (auto itr = used_pids_.end(); itr != used_pids_.begin(); itr--)
  {
    if(((UserThread*)currentThread)->getProcess()->getPID() != *itr)
    {
      return *itr;
    }
  }
  return (size_t)-1UL;
}

size_t ProcessRegistry::getPidAvailable()
{
  pid_available_lock_.acquire();
  pid_available_cond_.waitAndRelease();

  return last_pid_available_;
}

void ProcessRegistry::signalPidAvailable(size_t pid)
{
  pid_available_lock_.acquire();
  pid_available_cond_.broadcast();
  last_pid_available_ = pid;
  pid_available_lock_.release();
}

bool ProcessRegistry::checkPidWaitsHasDeadlock(size_t curr_pid, size_t pid_to_wait)
{
  MutexLock lock(pid_waits_lock_);
  for (auto itr = pid_waits_.begin(); itr != pid_waits_.end(); itr++)
  {
    PidWaits* pid_waits = *itr;
    if (curr_pid == pid_waits->getPid())
    {
      pid_waits->list_lock_.acquire();
      for (auto ptid_itr = pid_waits->getPtids()->begin(); ptid_itr != pid_waits->getPtids()->end(); ptid_itr++)
      {
        if(pid_to_wait == (*ptid_itr).pid_)
        {
          pid_waits->list_lock_.release();
          return true;
        }
      }
      pid_waits->list_lock_.release();
      
    }
  }

  return false;
}