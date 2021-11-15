
#pragma once

#include "Mutex.h"

class PageInfo
{
public:
  PageInfo();
  ~PageInfo();

  uint64 getRefCount();
  void setRefCount(uint64 value);

private:
  uint64 ref_count;
};