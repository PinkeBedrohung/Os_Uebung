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
        fd_(VfsSyscall::open(filename, O_RDONLY)), pid_(pid), filename_(filename), fs_info_(fs_info), terminal_number_(terminal_number)
{
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process
  
  if (fd_ >= 0)
      loader_ = new Loader(fd_);

  thread_ = new UserThread(this);
}

UserProcess::~UserProcess()
{
  debug(USERPROCESS, "Destructor UserProcess - PID %zu\n", getPID());

  delete loader_;
  loader_ = 0;

  if (fd_ > 0)
      VfsSyscall::close(fd_);

  ProcessRegistry::instance()->releasePID(pid_);
  ProcessRegistry::instance()->processExit();
}

Thread* UserProcess::getThread(){
  return thread_;
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