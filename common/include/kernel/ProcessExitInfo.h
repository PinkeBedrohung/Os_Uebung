#pragma once
#include "types.h"
#include "debug.h"

class ProcessExitInfo
{
public:
    ProcessExitInfo(size_t exit_val, size_t pid);
    ~ProcessExitInfo();

    ProcessExitInfo(const ProcessExitInfo &other);

    size_t exit_val_;
    size_t pid_;

};