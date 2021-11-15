
#pragma once

#include "Mutex.h"

class PageInfo
{
public:
  PageInfo();
  ~PageInfo();

  uint64 getRefCount();
  void setRefCount(uint64 value);
  void incRefCount();
  bool decRefCount();
  Mutex *getRefLock();
  void lockRefCount();
  void unlockRefCount();

private:
  uint64 ref_count_;
  Mutex ref_lock_;
};