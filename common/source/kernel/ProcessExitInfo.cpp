#include "ProcessExitInfo.h"

ProcessExitInfo::ProcessExitInfo(size_t exit_val, size_t pid) : exit_val_(exit_val), pid_(pid)
{
}

ProcessExitInfo::~ProcessExitInfo()
{
}

