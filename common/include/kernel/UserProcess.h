#pragma once

#include "Thread.h"
#include "ulist.h"
#include "Mutex.h"
#include "UserThread.h"

class UserThread;
#include "umap.h"

class UserProcess
{
  public:
    typedef ustl::list<Thread *> ThreadList;
    /**
     * Constructor
     * @param minixfs_filename filename of the file in minixfs to execute
     * @param fs_info filesysteminfo-object to be used
     * @param terminal_number the terminal to run in (default 0)
     *
     */
    UserProcess(ustl::string minixfs_filename, FileSystemInfo *fs_info, uint32 pid, uint32 terminal_number = 0);
    UserProcess(UserProcess &process, UserThread *thread, int* retval);

    virtual ~UserProcess();

    int32 getFd();
    ThreadList* getThreads();
    Thread* getThread(size_t tid);
    size_t getPID();
    void setPID(size_t pid);
    ustl::string getFilename();
    Loader *getLoader();
    uint32 getTerminalNumber();
    FileSystemInfo *getFsInfo();

    void addThread(Thread *thread);
    void removeThread(Thread *thread);
    uint64 copyPages();
    size_t getNumThreads();
    //Mutex* fork_lock_;
    //ustl::list<UserProcess *> *cow_holding_ps;
    void cancelNonCurrentThreads(Thread *thread);
    int  replaceProcessorImage(const char *path,char const* arg[]);
    //Mutex* fork_lock_;
    //ustl::list<UserProcess *> *cow_holding_ps;
    // bool holding_cow_;

    static const size_t MAX_PID = 4194304;
    size_t createUserThread(size_t* tid, void* (*routine)(void*), void* args, void* entry_function);
    size_t cancelUserThread(size_t tid);
    ustl::map<size_t, void*> retvals_;
    unsigned long long cpu_start_rdtsc = 0;
    void mapRetVals(size_t tid, void* retval);
    Mutex alive_lock_;
    Mutex threads_lock_;
    Mutex retvals_lock_;
    Mutex available_offsets_lock_;

    void freePageOffset(size_t offset);
    size_t getAvailablePageOffset();
    size_t getVPageOffset();
    void clearAvailableOffsets();

  private:
    int32 fd_;
    uint32 pid_;
    ustl::string filename_;
    Loader *loader_;
    FileSystemInfo *fs_info_;
    uint32 terminal_number_;
    UserProcess* parent_process_;
    ustl::list<UserProcess*> child_processes_;

    int32* binary_fd_counter_;

    ThreadList threads_;
    size_t num_threads_;

    size_t vpage_offset_;
    ustl::list<size_t> available_offsets_;  
};
