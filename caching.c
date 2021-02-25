#include <stdio.h>
#include <stdlib.h>
#include "memory_system.h"
/* Feel free to implement additional header files as needed */

typedef struct tlbStruct{
  int valid;
  int tag;
  int PPN;
} tlbItem;

typedef struct pageTableStruct{
  int valid;
  int PPN;
} pageTableItem;

typedef struct cacheStruct{
  int valid;
  int tag;
  int data;
  int time;
} cacheItem;

static tlbItem tlb[16];
static pageTableItem pgtable[512];
static cacheItem cache[32][2];

/*helper function to initialize TLB*/
void initializeTlb(){
  for (int i = 0; i < 16; i++){
    tlb[i].valid = 0;
    tlb[i].tag = 0;
    tlb[i].PPN = 0;
  }
}

/*helper function to initialize Page Table*/
void initializePageTable(){
  for (int i = 0; i < 512; i++){
    pgtable[i].valid = 0;
    pgtable[i].PPN = 0;
  }
}

/*helper function to initialize Cache*/
void initializeCache(){
  for (int i = 0; i < 32; i++){
    for (int j = 0; j < 2; j++){
      cache[i][j].valid = 0;
      cache[i][j].tag = 0;
      cache[i][j].data = 0;
      cache[i][j].time = 0;
    }
  }
}

/*TLB, Page Table and Cache initialization*/
void initialize(){
  initializeTlb();
  initializePageTable();
  initializeCache();
}

/*convert the incoming virtual address to a physical address*/
int get_physical_address(int virt_address){
  int PA; //physical address
  int temp = virt_address >> 18;
  //if the virt_address is too large
  if (temp > 0){
    log_entry(ILLEGALVIRTUAL,virt_address);
    return -1;
  }
  //convert
  int VPN = virt_address >> 9;
  int VPO = virt_address & 511;
  int TLBTag = VPN >> 5;
  int TLBIndex = VPN & 15; 
  //case 1: PPN is in TLB
  if (tlb[TLBIndex].valid == 1 && tlb[TLBIndex].tag == TLBTag){
    PA = (tlb[TLBIndex].PPN << 9) | VPO;
    log_entry(ADDRESS_FROM_TLB,PA);
  }
  //case 2: PPN is not in TLB, use the page table
  if (tlb[TLBIndex].valid == 0 || tlb[TLBIndex].tag != TLBTag){
    int PPN;
    //case 1: PPN is in pagetable
    if (pgtable[VPN].valid == 1){
      PPN = pgtable[VPN].PPN;
      tlb[TLBIndex].valid = 1;
      tlb[TLBIndex].tag = TLBTag;
      tlb[TLBIndex].PPN = PPN;
      PA = (PPN << 9) | VPO;
      log_entry(ADDRESS_FROM_PAGETABLE,PA);
    }
    //case 2: PPN is not in pagetable, load it from frame
    else {
      PPN = load_frame(VPN);
      pgtable[VPN].valid = 1;
      pgtable[VPN].PPN = PPN;
      tlb[TLBIndex].valid = 1;
      tlb[TLBIndex].tag = TLBTag;
      tlb[TLBIndex].PPN = PPN;
      PA = (PPN << 9) | VPO;
      log_entry(ADDRESS_FROM_PAGE_FAULT_HANDLER,PA); 
    }
  }
  return PA;
}

/*use the incoming physical address to find the relevent byte*/
char get_byte(int phys_address){
  char byte;
  //if phys_address is too large
  int temp = phys_address >> 20;
  if (temp > 0){
    return -1;
  }
  //convert
  int tag = phys_address >> 7;
  int index = (phys_address & 124) >> 2;
  int offset = phys_address & 3; // use data >> (8 * offset) for specific char
  //case 1: data is in cache
  if (cache[index][0].tag == tag || cache[index][1].tag == tag){
    if (cache[index][0].tag == tag && cache[index][0].valid == 1){
      byte = cache[index][0].data >> (8 * offset);
      log_entry(DATA_FROM_CACHE,byte);
    }
    else if (cache[index][1].tag == tag && cache[index][1].valid == 1){
      byte = cache[index][1].data >> (8 * offset);
      log_entry(DATA_FROM_CACHE,byte);
    }
  }
  //case 2: data is not in cache, not valid
  else {
    int membyte = get_word(phys_address);
    //if both entries are invalid, use the first one
    if (cache[index][0].valid == 0 && cache[index][1].valid == 0){
      cache[index][0].tag = tag;
      cache[index][0].data = membyte;
      cache[index][0].valid = 1;
      cache[index][0].time = 1;
      cache[index][1].time = 0;
      byte = cache[index][0].data >> (8 * offset);
      log_entry(DATA_FROM_MEMORY,byte);
    }
    //if one entry is invalid, use the invalid one
    else if (cache[index][0].valid == 0 && cache[index][1].valid == 1){
      cache[index][0].tag = tag;
      cache[index][0].data = membyte;
      cache[index][0].valid = 1;
      cache[index][0].time = 1; //this is newer
      cache[index][1].time = 0; //this is older
      byte = cache[index][0].data >> (8 * offset);
      log_entry(DATA_FROM_MEMORY,byte);
    }
    else if (cache[index][0].valid == 1 && cache[index][1].valid == 0){
      cache[index][1].tag = tag;
      cache[index][1].data = membyte;
      cache[index][1].valid = 1;
      cache[index][1].time = 1; //this is newer
      cache[index][0].time = 0; //this is older
      byte = cache[index][1].data >> (8 * offset);
      log_entry(DATA_FROM_MEMORY,byte);
    }
    //if both entries are valid, use the oldest one
    else if (cache[index][0].valid == 1 && cache[index][1].valid == 1){
      int older;
      if (cache[index][0].time == 0){
        older = 0;
      }
      else {
        older = 1;
      }
      cache[index][older].tag = tag;
      cache[index][older].data = membyte;
      cache[index][older].valid = 1;
      cache[index][older].time = 1; //make new
      cache[index][!older].time = 0; //make old
      byte = cache[index][older].data >> (8 * offset);
      log_entry(DATA_FROM_MEMORY,byte);
    }
    return byte;
  }
}
