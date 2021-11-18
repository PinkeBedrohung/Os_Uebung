#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "ulist.h"
#include "UserProcess.h"
#include "ProcessExitInfo.h"
#include "PidWaits.h"

class ProcessRegistry : public Thread
{
  public:
    /**
     * Constructor
     * @param root_fs_info the FileSystemInfo
     * @param progs a string-array of the userprograms which should be executed
     */
    ProcessRegistry ( FileSystemInfo *root_fs_info, char const *progs[] );
    ~ProcessRegistry();

    /**
     * Mounts the Minix-Partition with user-programs and creates processes
     */
    virtual void Run();

    /**
     * Tells us that a userprocess is being destroyed
     */
    void processExit();

    /**
     * Tells us that a userprocess is being created due to a fork or something similar
     */
    void processStart();

    /**
     * Tells us how many processes are running
     */
    size_t processCount();

    static ProcessRegistry* instance();
    void createProcess(const char* path);
    void createProcess(UserProcess *process);

    size_t getNewPID();
    void releasePID(size_t pid);
    bool checkProcessIsUsed(size_t pid);
    bool checkProcessIsZombie(size_t pid);
    void lockLists();
    void unlockLists();
    void addExitInfo(ProcessExitInfo &pexit_info);
    ProcessExitInfo getExitInfo(size_t pid, bool delete_entry=false);
    void makeZombiePID(size_t pid);

    Mutex pid_waits_lock_;
    ustl::list<PidWaits*> pid_waits_;

  private:

    char const **progs_;
    uint32 progs_running_;
    Mutex counter_lock_;
    Condition all_processes_killed_;
    static ProcessRegistry* instance_;

    Mutex list_lock_;
    Mutex exit_info_lock_;
    ustl::list<size_t> used_pids_;
    ustl::list<size_t> zombie_pids_;
    ustl::list<size_t> unused_pids_;
    ustl::list<ProcessExitInfo> pexit_infos_;
    size_t progs_started_;
};
