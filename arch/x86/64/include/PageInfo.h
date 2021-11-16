
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
  bool isPageFree();
  void setPageFree(bool cond);
  /*
  uint64 getUnsafeRefCount();
  void setUnsafeRefCount(uint64 value);
  bool decUnsafeRefCount();
*/
private:
  uint64 ref_count_;
  Mutex ref_lock_;
  bool page_free_;
};