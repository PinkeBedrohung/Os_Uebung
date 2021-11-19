#include "ProcessExitInfo.h"
#include "ProcessRegistry.h"

ProcessExitInfo::ProcessExitInfo(size_t exit_val, size_t pid) : exit_val_(exit_val), pid_(pid)
{
}

ProcessExitInfo::~ProcessExitInfo()
{
    debug(WAIT_PID, "~ProcessExitInfo - PID: %ld\n", pid_);
}

ProcessExitInfo::ProcessExitInfo(const ProcessExitInfo& other)
{
    exit_val_ = other.exit_val_;
    pid_ = other.pid_;
}
