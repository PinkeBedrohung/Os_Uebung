#include "ProcessExitInfo.h"

ProcessExitInfo::ProcessExitInfo(size_t exit_val, size_t pid) : exit_val_(exit_val), pid_(pid)
{
}

ProcessExitInfo::~ProcessExitInfo()
{
}

ProcessExitInfo::ProcessExitInfo(const ProcessExitInfo& other)
{
    exit_val_ = other.exit_val_;
    pid_ = other.pid_;
}
