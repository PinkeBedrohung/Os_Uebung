#pragma once

#include "Thread.h"
#include "ulist.h"
#include "Mutex.h"

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

    virtual ~UserProcess();

    int32 getFd();
    ThreadList* getThreads();
    size_t getPID();
    ustl::string getFilename();
    Loader *getLoader();
    uint32 getTerminalNumber();
    FileSystemInfo *getFsInfo();

    void add_thread(Thread *thread);
    void remove_thread(Thread *thread);
    size_t getNumThreads();
    static const size_t MAX_PID = 4194304;

  private:
    int32 fd_;
    uint32 pid_;
    ustl::string filename_;
    Loader *loader_;
    Thread *thread_;
    FileSystemInfo *fs_info_;
    uint32 terminal_number_;

    Mutex threads_lock_;
    ThreadList threads_;
    size_t num_threads_;
};
