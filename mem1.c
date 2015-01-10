#include "mem.h"
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

int arrl = 1;
int *bmask = 0;

void* map = 0;
int num_16 = -16;

int Mem_Init(int size)
{
	// Return failure if Mem_Init is called more than once
	if(num_16 >= 0 || size <=0)
		return -1;
	
	int page_size = getpagesize();
	// Round up size
	if(size % page_size != 0) 
		size = size + (page_size - (size % page_size));
	
  // open the /dev/zero device
  int fd = open("/dev/zero", O_RDWR);	
	// Get memory of size rounded_size
	map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  
  if(map == MAP_FAILED) return -1;

  //get 8 byte aligned
	if((uintptr_t)map  % 8 != 0) {
	  map += 8 - ((uintptr_t)map % 8);
		size -= 8 - ((uintptr_t)map % 8);
	}
	
  //get number of 16B slots in memory
  num_16 = size/16;
  if (num_16 % (sizeof(int)*8) == 0) {
    arrl = num_16/(sizeof(int)*8) ;
  }
  else
    arrl = num_16/(sizeof(int)*8) + 1;
   
  bmask = map; //bmask points to arrl long long int. 
  num_16 -= arrl/4; //this is the number of 16B slots available
  memset(bmask, 0, arrl*sizeof(int));

  map = map + arrl*sizeof(int);
  //printf("map = %p ; arrl = %d ; size = %d ; num_16 = %d \n", map, arrl, size, num_16);
  close(fd);
  return 0;	
}

int BitCount(unsigned int u)
{
  unsigned int uCount;
  uCount = u
  - ((u >> 1) & 033333333333)
  - ((u >> 2) & 011111111111);
  return
  ((uCount + (uCount >> 3))
  & 030707070707) % 63;
}

int First0Bit(int i)
{
  i=~i;
  return BitCount((i&(-i))-1);
}

void* Mem_Alloc(int size) {
  
  if (size <= 0 || num_16 == 0 ) //num_16=0 -> no free 16B slots
    return NULL;

  if(size % 8 != 0)
		size += 8 - (size % 8);

  //iterate over bitmask to find the first free slot.
  int i;
  int pos;
  void* user;
  for(i=0; i < arrl; i++){
    pos = First0Bit(*(bmask+i));
    if (pos == 32) //nothing free
      continue;
    *(bmask+i) = *(bmask+i) | 1 << pos;
    //printf("Allocating mem=%d ; pos = %d in bmask index %d. Free slot number = %d \n", size, pos, i, 32*i+pos);
    user = map + (32*i + pos)*16;
    num_16--;
    return user;
  }
}

int Mem_Free(void* ptr) {
  if (ptr == NULL)
    return -1;
  if ((uintptr_t)ptr % 16 != 0) //every pointer returned by Mem_Alloc will be 16B aligned
    return -1;
   
  int slot = (ptr - map)/16;
  printf("slot to be freed = %d \n", slot);
  int index = slot/32;
  if(index >= arrl) return -1;
  int pos = slot%32;
  //check if the pos is allocated, i.e. not free
  int val = *(bmask+index);
  val = (val & (1<<pos))>>pos;
  if (!val)
    return -1;
  int clear = ~(1 << pos);
  (*(bmask+index))&=clear;
  num_16++;
  printf("cleared pos %d in index %d \n", pos, index);
  return 0;
}

int Mem_Available() {
  printf("free memory = %dB \n", num_16*16);
  return num_16*16;
}
void Mem_Dump() {
  printf("status of bitmasks : \n");
  int i;
  for(i=0;i<arrl;i++) printf("0x%x\n", *(bmask+i));
}

