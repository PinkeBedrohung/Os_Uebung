
#pragma once

#include "Mutex.h"

class PageInfo
{
public:
  PageInfo();
  ~PageInfo();

  int64 getRefCount();
  void setRefCount(uint64 value);

private:
  int64 ref_count;
};