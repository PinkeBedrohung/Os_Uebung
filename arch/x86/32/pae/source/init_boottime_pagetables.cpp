/**
 * @file init_boottime_pagetables.cpp
 *
 */

#include "types.h"
#include "paging-definitions.h"
#include "offsets.h"
#include "multiboot.h"
#include "ArchCommon.h"
#include "assert.h"
#include "kprintf.h"
#include "ArchMemory.h"

extern void* kernel_end_address;

uint32 fb_start_hack = 0;

uint32 isPageUsed(uint32 page_number)
{
   uint32 i;
   uint32 num_modules = ArchCommon::getNumModules(0);
   for (i=0;i<num_modules;++i)
   {
      uint32 start_page = ArchCommon::getModuleStartAddress(i,0) / PAGE_SIZE;
      uint32 end_page = ArchCommon::getModuleEndAddress(i,0) / PAGE_SIZE;

      if ( start_page <= page_number && end_page >= page_number)
      {
         return 1;
      }
   }

   return 0;
}

uint32 getNextFreePage(uint32 page_number)
{
   while(isPageUsed(page_number))
     page_number++;
   return page_number;
}

// Be careful, this works because the beloved compiler generates
// relative calls in this case.
// if the compiler generated an absolut call we'd be screwed since we
// have not set up paging yet :)
extern "C" void initialiseBootTimePaging()
{
  uint32 i;

  PageDirPointerTableEntry *pdpt_start = (PageDirPointerTableEntry*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)kernel_page_directory_pointer_table);
  PageDirEntry *pde_start = (PageDirEntry*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)kernel_page_directory);
  for (i = 0; i < 4; ++i)
  {
    pdpt_start[i].page_directory_ppn = ((uint32) pde_start) / PAGE_SIZE + i;
    pdpt_start[i].present = 1;
  }

  //uint8 *pde_start_bytes = (uint8 *)pde_start;
  PageTableEntry *pte_start = (PageTableEntry*)VIRTUAL_TO_PHYSICAL_BOOT((pointer)kernel_page_tables);

  uint32 kernel_last_page = VIRTUAL_TO_PHYSICAL_BOOT((pointer)&kernel_end_address)/PAGE_SIZE;
  uint32 first_free_page = kernel_last_page + 1;

  // we do not have to clear the pde since its in the bss
  for (i = 0; i < 10; ++i)
  {
    pde_start[i].page.present = 1;
    pde_start[i].page.writeable = 1;
    pde_start[i].page.size = 1;
    pde_start[i].page.page_ppn = i;
  }

  for (i=0;i<8;++i)
  {
    pde_start[i+1024].pt.present = 1;
    pde_start[i+1024].pt.writeable = 1;
    pde_start[i+1024].pt.page_table_ppn = ((pointer)&pte_start[PAGE_TABLE_ENTRIES*i])/PAGE_SIZE;
  }

  // ok, we currently only fill in mappings for the first 4 megs (aka one page table)
  // we do not have to zero out the other page tables since they're already empty
  // thanks to the bss clearance.

  // update, from now on, all pages up to the last page containing only rodata
  // will be write protected.

  extern uint32 ro_data_end_address;

  uint32 last_ro_data_page = VIRTUAL_TO_PHYSICAL_BOOT((pointer)&ro_data_end_address)/PAGE_SIZE;

  // ppns are 1mb = 256 pages after vpns...
  for (i=0;i<last_ro_data_page-256;++i)
  {
    pte_start[i].present = 1;
    pte_start[i].writeable = 0;
    pte_start[i].page_ppn = i+256;
  }
  for (i=last_ro_data_page;i<first_free_page-256;++i)
  {
    pte_start[i].present = 1;
    pte_start[i].writeable = 1;
    pte_start[i].page_ppn = i+256;
  }

  if (ArchCommon::haveVESAConsole(0))
  {
    for (i=0;i<8;++i)
    {
      pde_start[1528+i].page.present = 1;
      pde_start[1528+i].page.writeable = 1;
      pde_start[1528+i].page.size = 1;
      pde_start[1528+i].page.cache_disabled = 1;
      pde_start[1528+i].page.write_through = 1;
      pde_start[1528+i].page.page_ppn = (ArchCommon::getVESAConsoleLFBPtr(0) / (1024*1024*4))+i;
    }
  }

  for (i=0;i<PAGE_DIRECTORY_ENTRIES;++i)
  {
    pde_start[i+PAGE_DIRECTORY_ENTRIES*3].page.present = 1;
    pde_start[i+PAGE_DIRECTORY_ENTRIES*3].page.writeable = 1;
    pde_start[i+PAGE_DIRECTORY_ENTRIES*3].page.size = 1;
    pde_start[i+PAGE_DIRECTORY_ENTRIES*3].page.page_ppn = i;
  }
}

extern "C" void removeBootTimeIdentMapping()
{
  uint32 i;

  for (i=0;i<10;++i)
  {
    kernel_page_directory[i].page.present=0;
  }
}
