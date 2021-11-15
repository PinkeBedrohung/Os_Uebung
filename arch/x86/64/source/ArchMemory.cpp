#include "ArchMemory.h"
#include "ArchInterrupts.h"
#include "kprintf.h"
#include "assert.h"
#include "PageManager.h"
#include "kstring.h"
#include "ArchThreads.h"
#include "Thread.h"
#include "Loader.h"
#include "Mutex.h"
#include "PageInfo.h"

PageMapLevel4Entry kernel_page_map_level_4[PAGE_MAP_LEVEL_4_ENTRIES] __attribute__((aligned(0x1000)));
PageDirPointerTableEntry kernel_page_directory_pointer_table[2 * PAGE_DIR_POINTER_TABLE_ENTRIES] __attribute__((aligned(0x1000)));
PageDirEntry kernel_page_directory[2 * PAGE_DIR_ENTRIES] __attribute__((aligned(0x1000)));
PageTableEntry kernel_page_table[8 * PAGE_TABLE_ENTRIES] __attribute__((aligned(0x1000)));

PageInfo pageInfo[2048];
//uint32 access_counter[2048] = {}; // Initialize to 0
Mutex archmem_lock("ArchMemory::archmem_lock");

ArchMemory::ArchMemory()
{
  page_map_level_4_ = PageManager::instance()->allocPPN();
  PageMapLevel4Entry* new_pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  memcpy((void*) new_pml4, (void*) kernel_page_map_level_4, PAGE_SIZE);
  memset(new_pml4, 0, PAGE_SIZE / 2); // should be zero, this is just for safety
}

ArchMemory::ArchMemory(ArchMemory &archmemory)
{
  page_map_level_4_ = PageManager::instance()->allocPPN();
  PageMapLevel4Entry* new_pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(page_map_level_4_);
  memcpy((void*) new_pml4, (void*) kernel_page_map_level_4, PAGE_SIZE);
  memset(new_pml4, 0, PAGE_SIZE / 2); // should be zero, this is just for safety

  // Copies PML4, PDPT, PD, PT and sets the PT entries to writeable 0 on both structures
  // Page PPN in PT entries are the same in both paging structures
  copyPagingStructure(archmemory.page_map_level_4_, page_map_level_4_);
}

template<typename T>
bool ArchMemory::checkAndRemove(pointer map_ptr, uint64 index)
{
  T* map = (T*) map_ptr;
  debug(A_MEMORY, "%s: page %p index %zx\n", __PRETTY_FUNCTION__, map, index);
  ((uint64*) map)[index] = 0;
  for (uint64 i = 0; i < PAGE_DIR_ENTRIES; i++)
  {
    if (map[i].present != 0)
      return false;
  }
  return true;
}

bool ArchMemory::unmapPage(uint64 virtual_page)
{
  ArchMemoryMapping m = resolveMapping(virtual_page);

  assert(m.page_ppn != 0 && m.page_size == PAGE_SIZE && m.pt[m.pti].present);

  //pageInfo[m.pt[m.pti].page_ppn].lockRefCount();
  m.pt[m.pti].present = 0;

  pageInfo[m.pt[m.pti].page_ppn].decUnsafeRefCount();

  debug(COW, "Unmap Page REF Count: %ld, PPN: %ld\n", pageInfo[m.pt[m.pti].page_ppn].getUnsafeRefCount(), m.pt[m.pti].page_ppn);
  if(pageInfo[m.pt[m.pti].page_ppn].getUnsafeRefCount() == 0)
  {
    debug(COW, "Unmap Page PPN: %ld\n", m.pt[m.pti].page_ppn);
    PageManager::instance()->freePPN(m.page_ppn);
  }
  //pageInfo[m.pt[m.pti].page_ppn].unlockRefCount();

  ((uint64*)m.pt)[m.pti] = 0; // for easier debugging
  bool empty = checkAndRemove<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti);
  if (empty)
  {
    empty = checkAndRemove<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi);
    PageManager::instance()->freePPN(m.pt_ppn);
  }
  if (empty)
  {
    empty = checkAndRemove<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti);
    PageManager::instance()->freePPN(m.pd_ppn);
  }
  if (empty)
  {
    empty = checkAndRemove<PageMapLevel4Entry>(getIdentAddressOfPPN(m.pml4_ppn), m.pml4i);
    PageManager::instance()->freePPN(m.pdpt_ppn);
  }
  return true;
}

template<typename T>
bool ArchMemory::insert(pointer map_ptr, uint64 index, uint64 ppn, uint64 bzero, uint64 size, uint64 user_access,
                        uint64 writeable)
{
  assert(map_ptr & ~0xFFFFF00000000000ULL);
  T* map = (T*) map_ptr;
  debug(A_MEMORY, "%s: page %p index %zx ppn %zx user_access %zx size %zx\n", __PRETTY_FUNCTION__, map, index, ppn,
        user_access, size);
  if (bzero)
  {
    memset((void*) getIdentAddressOfPPN(ppn), 0, PAGE_SIZE);
    assert(((uint64* )map)[index] == 0);
  }
  map[index].size = size;
  map[index].writeable = writeable;
  map[index].page_ppn = ppn;
  map[index].user_access = user_access;
  map[index].present = 1;
  map[index].access_ctr = 0;
  return true;
}

bool ArchMemory::mapPage(uint64 virtual_page, uint64 physical_page, uint64 user_access)
{
  MutexLock lock(archmem_lock);
  debug(A_MEMORY, "%zx %zx %zx %zx\n", page_map_level_4_, virtual_page, physical_page, user_access);
  ArchMemoryMapping m = resolveMapping(page_map_level_4_, virtual_page);
  assert((m.page_size == 0) || (m.page_size == PAGE_SIZE));

  if (m.pdpt_ppn == 0)
  {
    m.pdpt_ppn = PageManager::instance()->allocPPN();
    insert<PageMapLevel4Entry>((pointer) m.pml4, m.pml4i, m.pdpt_ppn, 1, 0, 1, 1);
  }

  if (m.pd_ppn == 0)
  {
    m.pd_ppn = PageManager::instance()->allocPPN();
    insert<PageDirPointerTablePageDirEntry>(getIdentAddressOfPPN(m.pdpt_ppn), m.pdpti, m.pd_ppn, 1, 0, 1, 1);
  }

  if (m.pt_ppn == 0)
  {
    m.pt_ppn = PageManager::instance()->allocPPN();
    insert<PageDirPageTableEntry>(getIdentAddressOfPPN(m.pd_ppn), m.pdi, m.pt_ppn, 1, 0, 1, 1);
  }

  if (m.page_ppn == 0)
  {
    return insert<PageTableEntry>(getIdentAddressOfPPN(m.pt_ppn), m.pti, physical_page, 0, 0, user_access, 1);
  }

  return false;
}

ArchMemory::~ArchMemory()
{
  assert(currentThread->kernel_registers_->cr3 != page_map_level_4_ * PAGE_SIZE && "thread deletes its own arch memory");
  MutexLock lock(archmem_lock);
  PageMapLevel4Entry *pml4 = (PageMapLevel4Entry *)getIdentAddressOfPPN(page_map_level_4_);
  for (uint64 pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++) // free only lower half
  {
    if (pml4[pml4i].present)
    {
      PageDirPointerTableEntry* pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[pml4i].page_ppn);
      for (uint64 pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if (pdpt[pdpti].pd.present)
        {
          assert(pdpt[pdpti].pd.size == 0);
          PageDirEntry* pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (uint64 pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if (pd[pdi].pt.present)
            {
              assert(pd[pdi].pt.size == 0);
              PageTableEntry* pt = (PageTableEntry*) getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (uint64 pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                if (pt[pti].present)
                {
                  //pageInfo[pt[pti].page_ppn].lockRefCount();
                  pt[pti].present = 0;

                  pageInfo[pt[pti].page_ppn].decUnsafeRefCount();
                  debug(COW, "REF Count: %ld, PPN: %ld\n", pageInfo[pt[pti].page_ppn].getUnsafeRefCount(), pt[pti].page_ppn);

                  if(pageInfo[pt[pti].page_ppn].getUnsafeRefCount() == 0)
                  {
                    debug(COW, "Free page PPN: %ld\n", pt[pti].page_ppn);
                    PageManager::instance()->freePPN(pt[pti].page_ppn);
                  }
                  //pageInfo[pt[pti].page_ppn].unlockRefCount();
                }
              }
              pd[pdi].pt.present = 0;
              PageManager::instance()->freePPN(pd[pdi].pt.page_ppn);
            }
          }
          pdpt[pdpti].pd.present = 0;
          PageManager::instance()->freePPN(pdpt[pdpti].pd.page_ppn);
        }
      }
      pml4[pml4i].present = 0;
      PageManager::instance()->freePPN(pml4[pml4i].page_ppn);
    }
  }
  PageManager::instance()->freePPN(page_map_level_4_);
}

pointer ArchMemory::checkAddressValid(uint64 vaddress_to_check)
{
  ArchMemoryMapping m = resolveMapping(page_map_level_4_, vaddress_to_check / PAGE_SIZE);
  if (m.page != 0)
  {
    debug(A_MEMORY, "checkAddressValid %zx and %zx -> true\n", page_map_level_4_, vaddress_to_check);
    return m.page | (vaddress_to_check % m.page_size);
  }
  else
  {
    debug(A_MEMORY, "checkAddressValid %zx and %zx -> false\n", page_map_level_4_, vaddress_to_check);
    return 0;
  }
}

const ArchMemoryMapping ArchMemory::resolveMapping(uint64 vpage)
{
  return resolveMapping(page_map_level_4_, vpage);
}

const ArchMemoryMapping ArchMemory::resolveMapping(uint64 pml4, uint64 vpage)
{
  ArchMemoryMapping m;

  m.pti = vpage;
  m.pdi = m.pti / PAGE_TABLE_ENTRIES;
  m.pdpti = m.pdi / PAGE_DIR_ENTRIES;
  m.pml4i = m.pdpti / PAGE_DIR_POINTER_TABLE_ENTRIES;

  m.pti %= PAGE_TABLE_ENTRIES;
  m.pdi %= PAGE_DIR_ENTRIES;
  m.pdpti %= PAGE_DIR_POINTER_TABLE_ENTRIES;
  m.pml4i %= PAGE_MAP_LEVEL_4_ENTRIES;

  assert(pml4 < PageManager::instance()->getTotalNumPages());
  m.pml4 = (PageMapLevel4Entry*) getIdentAddressOfPPN(pml4);
  m.pdpt = 0;
  m.pd = 0;
  m.pt = 0;
  m.page = 0;
  m.pml4_ppn = pml4;
  m.pdpt_ppn = 0;
  m.pd_ppn = 0;
  m.pt_ppn = 0;
  m.page_ppn = 0;
  m.page_size = 0;
  if (m.pml4[m.pml4i].present)
  {
    m.pdpt_ppn = m.pml4[m.pml4i].page_ppn;
    m.pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(m.pml4[m.pml4i].page_ppn);
    if (m.pdpt[m.pdpti].pd.present && !m.pdpt[m.pdpti].pd.size) // 1gb page ?
    {
      m.pd_ppn = m.pdpt[m.pdpti].pd.page_ppn;
      if (m.pd_ppn > PageManager::instance()->getTotalNumPages())
      {
        debug(A_MEMORY, "%zx\n", m.pd_ppn);
      }
      assert(m.pd_ppn < PageManager::instance()->getTotalNumPages());
      m.pd = (PageDirEntry*) getIdentAddressOfPPN(m.pdpt[m.pdpti].pd.page_ppn);
      if (m.pd[m.pdi].pt.present && !m.pd[m.pdi].pt.size) // 2mb page ?
      {
        m.pt_ppn = m.pd[m.pdi].pt.page_ppn;
        assert(m.pt_ppn < PageManager::instance()->getTotalNumPages());
        m.pt = (PageTableEntry*) getIdentAddressOfPPN(m.pd[m.pdi].pt.page_ppn);
        if (m.pt[m.pti].present)
        {
          m.page = getIdentAddressOfPPN(m.pt[m.pti].page_ppn);
          m.page_ppn = m.pt[m.pti].page_ppn;
          assert(m.page_ppn < PageManager::instance()->getTotalNumPages());
          m.page_size = PAGE_SIZE;
        }
      }
      else if (m.pd[m.pdi].page.present)
      {
        m.page_size = PAGE_SIZE * PAGE_TABLE_ENTRIES;
        m.page_ppn = m.pd[m.pdi].page.page_ppn;
        m.page = getIdentAddressOfPPN(m.pd[m.pdi].page.page_ppn);
      }
    }
    else if (m.pdpt[m.pdpti].page.present)
    {
      m.page_size = PAGE_SIZE * PAGE_TABLE_ENTRIES * PAGE_DIR_ENTRIES;
      m.page_ppn = m.pdpt[m.pdpti].page.page_ppn;
      assert(m.page_ppn < PageManager::instance()->getTotalNumPages());
      m.page = getIdentAddressOfPPN(m.pdpt[m.pdpti].page.page_ppn);
    }
  }
  return m;
}

void ArchMemory::writeable(uint64 pml4_ppn, int8 writeable, EntryCounter entr_ctr, uint64 entr_ctr_value)
{
  PageMapLevel4Entry *pml4 = (PageMapLevel4Entry*)ArchMemory::getIdentAddressOfPPN(pml4_ppn);
  PageDirPointerTableEntry *pdpt;
  PageDirEntry *pd;
  PageTableEntry *pt;
  size_t mapped_pages = 1;

  int8 addend = entr_ctr == Increment ? entr_ctr_value : entr_ctr == Decrement ? -entr_ctr_value : 0;

  writeable = writeable > 0 ? 1 : writeable < 0 ? -1 : 0;

  for (size_t pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++)
  {
    if(pml4[pml4i].present)
    {
      mapped_pages++;
      //debug(A_MEMORY, "PML4i %zu - PML4 Entry: %p Present bit: %zu\n", pml4i, &pml4[pml4i], pml4[pml4i].present);
      assert(pml4[pml4i].access_ctr >= 0);
      if(writeable != -1)
        pml4[pml4i].writeable = writeable;
      pml4[pml4i].access_ctr += addend;

      pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(pml4[pml4i].page_ppn);

      for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if(pdpt[pdpti].pd.present)
        {
          mapped_pages++;
          //debug(A_MEMORY, "\tPDPTi %zu - PDPT Entry: %p Present bit: %zu\n", pdpti, &pdpt[pdpti], pdpt[pdpti].pd.present);
          assert(pdpt[pdpti].pd.access_ctr >= 0);
          if(writeable != -1)
            pdpt[pdpti].pd.writeable = writeable;
          pdpt[pdpti].pd.access_ctr += addend;

          pd = (PageDirEntry *)ArchMemory::getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if(pd[pdi].pt.present)
            {
              mapped_pages++;
              //debug(A_MEMORY, "\t\tPDi %zu - PD Entry: %p Present bit: %zu\n", pdi, &pd[pdi], pd[pdi].pt.present);
              assert(pd[pdi].pt.access_ctr >= 0);
              if(writeable != -1)
                pd[pdi].pt.writeable = writeable;
              pd[pdi].pt.access_ctr += addend;

              pt = (PageTableEntry *)ArchMemory::getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                if(pt[pti].present)
                {
                  mapped_pages++;
                  //debug(A_MEMORY, "\t\t\tPTi %zu - PT Entry: %p Present bit: %zu\n", pti, &pt[pti], pt[pti].present);
                  assert(pt[pti].access_ctr >= 0);
                  if(writeable != -1)
                    pt[pti].writeable = writeable;
                  pt[pti].access_ctr += addend;
                }
              }
            }
          }
        }
      }
    }
  }
  debug(A_MEMORY, "Set Writeable bit to %d on %zu pages\n", writeable, mapped_pages);
}

uint64 ArchMemory::copyPagingStructure(uint64 pml4_ppn)
{
  PageMapLevel4Entry *pml4 = (PageMapLevel4Entry*)ArchMemory::getIdentAddressOfPPN(pml4_ppn);
  PageDirPointerTableEntry *pdpt;
  PageDirEntry *pd;
  PageTableEntry *pt;
  
  for (size_t pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++)
  {
    if(pml4[pml4i].present && pml4[pml4i].access_ctr == 0)
    {
      return pml4_ppn;
    }
  }

  uint64 new_pml4_ppn = PageManager::instance()->allocPPN();
  PageMapLevel4Entry *new_pml4 = (PageMapLevel4Entry*)ArchMemory::getIdentAddressOfPPN(new_pml4_ppn);
  PageDirPointerTableEntry *new_pdpt;
  PageDirEntry *new_pd;
  PageTableEntry *new_pt;

  for (size_t pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++)
  {
    if(pml4[pml4i].present)
    {
      
      assert(pml4[pml4i].access_ctr > 0);
      pml4[pml4i].access_ctr--;

      if(pml4[pml4i].access_ctr == 0)
        pml4[pml4i].writeable = 1;
      
      memcpy(&new_pml4[pml4i], &pml4[pml4i], sizeof(PageMapLevel4Entry));
      new_pml4[pml4i].writeable = 1;
      new_pml4[pml4i].access_ctr = 0;
      new_pml4[pml4i].page_ppn = PageManager::instance()->allocPPN();
      
      new_pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(new_pml4[pml4i].page_ppn);

      pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(pml4[pml4i].page_ppn);
      for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if(pdpt[pdpti].pd.present)
        {
          
          assert(pdpt[pdpti].pd.access_ctr > 0);
          pdpt[pdpti].pd.access_ctr--;

          if(pdpt[pdpti].pd.access_ctr == 0)
            pdpt[pdpti].pd.writeable = 1;
          
          memcpy(&new_pdpt[pdpti], &pdpt[pdpti], sizeof(PageDirPointerTableEntry));
          new_pdpt[pdpti].pd.writeable = 1;
          new_pdpt[pdpti].pd.access_ctr = 0;
          new_pdpt[pdpti].pd.page_ppn = PageManager::instance()->allocPPN();
          
          new_pd = (PageDirEntry *)getIdentAddressOfPPN(new_pdpt[pdpti].pd.page_ppn);

          pd = (PageDirEntry *)ArchMemory::getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if(pd[pdi].pt.present)
            {
              
              assert(pd[pdi].pt.access_ctr > 0);
              pd[pdi].pt.access_ctr--;

              if(pd[pdi].pt.access_ctr == 0)
                pd[pdi].pt.writeable = 1;
              
              memcpy(&new_pd[pdi], &pd[pdi], sizeof(PageDirEntry));
              new_pd[pdi].pt.writeable = 1;
              new_pd[pdi].pt.access_ctr = 0;
              new_pd[pdi].pt.page_ppn = PageManager::instance()->allocPPN();
                
              new_pt = (PageTableEntry *)getIdentAddressOfPPN(new_pd[pdi].pt.page_ppn);

              pt = (PageTableEntry *)ArchMemory::getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                if(pt[pti].present)
                {
                  
                  assert(pt[pti].access_ctr > 0);
                  pt[pti].access_ctr--;

                  if(pt[pti].access_ctr == 0)
                    pt[pti].writeable = 1;
                  
                  memcpy(&new_pt[pti], &pt[pti], sizeof(PageTableEntry));
                  new_pt[pti].writeable = 1;
                  new_pt[pti].access_ctr = 0;
                  new_pt[pti].page_ppn = PageManager::instance()->allocPPN();

                  memcpy((void*)getIdentAddressOfPPN(new_pt[pti].page_ppn), (void*)getIdentAddressOfPPN(pt[pti].page_ppn), PAGE_SIZE);
                }
              }
            }
          }
        }
      }
    }
  }
  
  ArchMemory::setKernelPagingPML4(pml4_ppn, new_pml4_ppn);
  return new_pml4_ppn;
}

void ArchMemory::setKernelPagingPML4(uint64 pml4_ppn, uint64 new_pml4_ppn)
{
  PageMapLevel4Entry *pml4 = (PageMapLevel4Entry*)ArchMemory::getIdentAddressOfPPN(pml4_ppn);
  PageMapLevel4Entry *new_pml4 = (PageMapLevel4Entry*)ArchMemory::getIdentAddressOfPPN(new_pml4_ppn);

  for (size_t pml4i = PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i < PAGE_MAP_LEVEL_4_ENTRIES; pml4i++)
  {
    if(pml4[pml4i].present)
    {
      memcpy(&new_pml4[pml4i], &pml4[pml4i], sizeof(PageMapLevel4Entry));
    }
  }
}

void ArchMemory::copyPagingStructure(uint64 pml4_ppn, uint64 new_pml4_ppn)
{
  MutexLock lock(archmem_lock);
  PageMapLevel4Entry *pml4 = (PageMapLevel4Entry *)getIdentAddressOfPPN(pml4_ppn);
  PageDirPointerTableEntry *pdpt;
  PageDirEntry *pd;
  PageTableEntry *pt;

  PageMapLevel4Entry *new_pml4 = (PageMapLevel4Entry*)getIdentAddressOfPPN(new_pml4_ppn);
  PageDirPointerTableEntry *new_pdpt;
  PageDirEntry *new_pd;
  PageTableEntry *new_pt;

  for (size_t pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++)
  {
    if(pml4[pml4i].present)
    {
      memcpy(&new_pml4[pml4i], &pml4[pml4i], sizeof(PageMapLevel4Entry));
      new_pml4[pml4i].writeable = 1;
      new_pml4[pml4i].page_ppn = PageManager::instance()->allocPPN();
      
      new_pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(new_pml4[pml4i].page_ppn);

      pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(pml4[pml4i].page_ppn);
      for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if(pdpt[pdpti].pd.present)
        {
          memcpy(&new_pdpt[pdpti], &pdpt[pdpti], sizeof(PageDirPointerTableEntry));
          new_pdpt[pdpti].pd.writeable = 1;
          new_pdpt[pdpti].pd.page_ppn = PageManager::instance()->allocPPN();
          
          new_pd = (PageDirEntry *)getIdentAddressOfPPN(new_pdpt[pdpti].pd.page_ppn);

          pd = (PageDirEntry *)getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if(pd[pdi].pt.present)
            {
              memcpy(&new_pd[pdi], &pd[pdi], sizeof(PageDirEntry));
              new_pd[pdi].pt.writeable = 1;
              new_pd[pdi].pt.page_ppn = PageManager::instance()->allocPPN();
                
              new_pt = (PageTableEntry *)getIdentAddressOfPPN(new_pd[pdi].pt.page_ppn);

              pt = (PageTableEntry *)getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                if(pt[pti].present)
                {
                  // Set the new and old pagetable entries to writeable 0
                  pageInfo[pt[pti].page_ppn].lockRefCount();
                  pt[pti].writeable = 0;
                  memcpy(&new_pt[pti], &pt[pti], sizeof(PageTableEntry));
                  
                  if(pageInfo[pt[pti].page_ppn].getRefCount() == 0)
                  {
                    pageInfo[pt[pti].page_ppn].setRefCount(2);
                  }
                  else
                  {
                    pageInfo[pt[pti].page_ppn].incRefCount();
                  }
                  pageInfo[pt[pti].page_ppn].unlockRefCount();
                }
              }
            }
          }
        }
      }
    }
  }
}

void ArchMemory::copyPage(uint64 pml4_ppn, uint64 address)
{
  MutexLock lock(archmem_lock);
  ArchMemoryMapping m = ArchMemory::resolveMapping(currentThread->loader_->arch_memory_.page_map_level_4_, address / PAGE_SIZE);
  uint64 page_ppn = m.page_ppn;
  
  PageMapLevel4Entry *pml4 = (PageMapLevel4Entry*)getIdentAddressOfPPN(pml4_ppn);
  PageDirPointerTableEntry *pdpt;
  PageDirEntry *pd;
  PageTableEntry *pt;

  pt = (PageTableEntry*)getIdentAddressOfPPN(m.pt_ppn);

  for (size_t pml4i = 0; pml4i < PAGE_MAP_LEVEL_4_ENTRIES / 2; pml4i++)
  {
    if(pml4[pml4i].present)
    {
      pdpt = (PageDirPointerTableEntry *)getIdentAddressOfPPN(pml4[pml4i].page_ppn);
      for (size_t pdpti = 0; pdpti < PAGE_DIR_POINTER_TABLE_ENTRIES; pdpti++)
      {
        if(pdpt[pdpti].pd.present)
        {
          pd = (PageDirEntry *)getIdentAddressOfPPN(pdpt[pdpti].pd.page_ppn);
          for (size_t pdi = 0; pdi < PAGE_DIR_ENTRIES; pdi++)
          {
            if(pd[pdi].pt.present)
            {
              pt = (PageTableEntry *)getIdentAddressOfPPN(pd[pdi].pt.page_ppn);
              for (size_t pti = 0; pti < PAGE_TABLE_ENTRIES; pti++)
              {
                if(pt[pti].present && pt[pti].page_ppn == page_ppn)
                {
                  pageInfo[pt[pti].page_ppn].lockRefCount();
                  assert(pageInfo[pt[pti].page_ppn].getRefCount() != 0);

                  if (pageInfo[pt[pti].page_ppn].getRefCount() != 0)
                    pageInfo[pt[pti].page_ppn].decRefCount();

                  debug(COW, "REF Counter: %ld\n",pageInfo[pt[pti].page_ppn].getRefCount());
                  
                  if(pageInfo[pt[pti].page_ppn].getRefCount() != 0)
                  {
                    uint64 old_page_ppn = pt[pti].page_ppn;

                    pt[pti].page_ppn = PageManager::instance()->allocPPN();

                    memcpy((void*)getIdentAddressOfPPN(pt[pti].page_ppn), (void*)getIdentAddressOfPPN(old_page_ppn), PAGE_SIZE);
                    pageInfo[old_page_ppn].unlockRefCount();
                    debug(COW, "Copied Page PPN: %ld to new PPN: %ld\n", old_page_ppn, pt[pti].page_ppn);
                  }
                  else
                  {
                    pageInfo[pt[pti].page_ppn].unlockRefCount();
                  }
                  pt[pti].writeable = 1;
                  
                  return;
                }
              }
            }
          }
        }
      }
    }
  }
}

void ArchMemory::printMemoryMapping(ArchMemoryMapping* mapping)
{
  debug(A_MEMORY, "PML4 Entry: %p\n",mapping->pml4);
  debug(A_MEMORY, "PDPT Entry: %p\n",mapping->pdpt);
  debug(A_MEMORY, "PD Entry: %p\n",mapping->pd);
  debug(A_MEMORY, "PT Entry: %p\n",mapping->pt);

  debug(A_MEMORY, "PML4 PPN: %zu\n",mapping->pml4_ppn);
  debug(A_MEMORY, "PDPT PPN: %zu\n",mapping->pdpt_ppn);
  debug(A_MEMORY, "PD PPN: %zu\n",mapping->pd_ppn);
  debug(A_MEMORY, "PT PPN: %zu\n",mapping->pt_ppn);

  debug(A_MEMORY, "PML4 Index: %zu\n",mapping->pml4i);
  debug(A_MEMORY, "PDPT Index: %zu\n",mapping->pdpti);
  debug(A_MEMORY, "PD Index: %zu\n",mapping->pdi);
  debug(A_MEMORY, "PT Index: %zu\n",mapping->pti);
}

size_t ArchMemory::get_PPN_Of_VPN_In_KernelMapping(size_t virtual_page, size_t *physical_page,
                                                   size_t *physical_pte_page)
{
  ArchMemoryMapping m = resolveMapping(((uint64) VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
                                       virtual_page);
  if (physical_page)
    *physical_page = m.page_ppn;
  if (physical_pte_page)
    *physical_pte_page = m.pt_ppn;
  return m.page_size;
}

void ArchMemory::mapKernelPage(size_t virtual_page, size_t physical_page)
{
  ArchMemoryMapping mapping = resolveMapping(((uint64) VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
                                             virtual_page);
  PageMapLevel4Entry* pml4 = kernel_page_map_level_4;
  assert(pml4[mapping.pml4i].present);
  PageDirPointerTableEntry *pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[mapping.pml4i].page_ppn);
  assert(pdpt[mapping.pdpti].pd.present);
  PageDirEntry *pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[mapping.pdpti].pd.page_ppn);
  assert(pd[mapping.pdi].pt.present);
  PageTableEntry *pt = (PageTableEntry*) getIdentAddressOfPPN(pd[mapping.pdi].pt.page_ppn);
  assert(!pt[mapping.pti].present);
  pt[mapping.pti].writeable = 1;
  pt[mapping.pti].page_ppn = physical_page;
  pt[mapping.pti].present = 1;
  asm volatile ("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax");
}

void ArchMemory::unmapKernelPage(size_t virtual_page)
{
  ArchMemoryMapping mapping = resolveMapping(((uint64) VIRTUAL_TO_PHYSICAL_BOOT(kernel_page_map_level_4) / PAGE_SIZE),
                                             virtual_page);
  PageMapLevel4Entry* pml4 = kernel_page_map_level_4;
  assert(pml4[mapping.pml4i].present);
  PageDirPointerTableEntry *pdpt = (PageDirPointerTableEntry*) getIdentAddressOfPPN(pml4[mapping.pml4i].page_ppn);
  assert(pdpt[mapping.pdpti].pd.present);
  PageDirEntry *pd = (PageDirEntry*) getIdentAddressOfPPN(pdpt[mapping.pdpti].pd.page_ppn);
  assert(pd[mapping.pdi].pt.present);
  PageTableEntry *pt = (PageTableEntry*) getIdentAddressOfPPN(pd[mapping.pdi].pt.page_ppn);
  assert(pt[mapping.pti].present);
  pt[mapping.pti].present = 0;
  pt[mapping.pti].writeable = 0;
  PageManager::instance()->freePPN(pt[mapping.pti].page_ppn);
  asm volatile ("movq %%cr3, %%rax; movq %%rax, %%cr3;" ::: "%rax");
}

uint64 ArchMemory::getRootOfPagingStructure()
{
  return page_map_level_4_;
}

PageMapLevel4Entry* ArchMemory::getRootOfKernelPagingStructure()
{
  return kernel_page_map_level_4;
}
