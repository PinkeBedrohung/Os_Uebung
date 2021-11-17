#pragma once
#include "types.h"

class ProcessExitInfo
{
public:
    ProcessExitInfo(size_t exit_val, size_t pid);
    ~ProcessExitInfo();

    size_t exit_val_;
    size_t pid_;

};