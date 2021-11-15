#include "PageInfo.h"
#include "assert.h"
#include "ArchThreads.h"


PageInfo::PageInfo(): ref_count_(0), ref_lock_("PageInfo::ref_lock_")
{
}

PageInfo::~PageInfo()
{
}

uint64 PageInfo::getRefCount()
{
    assert(ref_lock_.isHeldBy(currentThread));
    return ref_count_;
}

void PageInfo::setRefCount(uint64 value)
{
    assert(ref_lock_.isHeldBy(currentThread));
    ref_count_ = value;
}

void PageInfo::incRefCount()
{
    assert(ref_lock_.isHeldBy(currentThread));
    ref_count_++;
}

bool PageInfo::decRefCount()
{
    assert(ref_lock_.isHeldBy(currentThread));
    if(ref_count_ != 0)
    {
        ref_count_--;
        return true;
    }
    return false;
}

Mutex* PageInfo::getRefLock()
{
    return &ref_lock_;
}

void PageInfo::lockRefCount()
{
    ref_lock_.acquire();
}

void PageInfo::unlockRefCount()
{
    assert(ref_lock_.isHeldBy(currentThread));
    ref_lock_.release();
}
