#include "PageInfo.h"

PageInfo::PageInfo()
{

}

PageInfo::~PageInfo()
{
    
}

uint64 PageInfo::getRefCount()
{
    return ref_count;
}

void PageInfo::setRefCount(uint64 value)
{
    ref_count = value;
}
