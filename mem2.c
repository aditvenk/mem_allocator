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
int ret = -1;
void* map = 0;
int num_16 = -1;

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
    arrl = 2* num_16/(sizeof(int)*8) ; //each 16B slot needs 2 bits
  }
  else
    arrl = 2*(num_16/(sizeof(int)*8)) + 1;
   
  bmask = map; //bmask points to arrl long long int. 
  num_16 -= arrl/4; //this is the number of 16B slots available
  memset(bmask, 0, arrl*sizeof(int));

  map = map + arrl*sizeof(int);
  //printf("map = %p ; arrl = %d ; size = %d ; num_16 = %d \n", map, arrl, size, num_16);
  close(fd);
  return 0;	
}
int start_i = -1, start_pos = -1, end_i = -1, end_pos = -1;
int Find00Pos(void *x, int num) {
  //find num consecutive 00s (from LSB) and return pos of first
  int cnt = -1; 
  int i = 0, j = 0;
  start_pos = -1;
  start_i = -1;
  end_pos = -1;
  end_i = -1;
  unsigned int b;
  while(cnt <= num) {
    b = (*((unsigned int*)x+i)>>j)&3;
    if (b==0) {
      if(cnt == -1) {
        start_i = i;
        start_pos = j;
        cnt=0;
      }
      cnt++;
      if(cnt == num) {
        end_i = i;
        end_pos = j;
        return 0;
      }
    }
    else {
      cnt = -1;
      start_pos = -1;
      start_i = -1;
      end_pos = -1;
      end_i = -1;
    }
    if(b==1 || b==0) j+=2;
    if(b==2) j+=10;
    if(b==3) j+=32;

    if(j>=32) {
      i++;
      j=j%32;
      if (i>=arrl)
        break;
    }
  }
  return -1;
}
int SetVal(void* x, int val) {
  //put val from start_i/pos to end_i/pos
  int s_i = start_i;
  int s_pos = start_pos;
  int e_i = end_i;
  int e_pos = end_pos;
  while (1) {
    if (val != 0) 
      *((int*)x+s_i) |= (val<<s_pos);
    else 
      *((int*)x+s_i) &= ~(3<<s_pos);
    s_pos+=2;
    if (s_pos >= 32) {
      s_i++;
      s_pos %= s_pos;
      if (s_i > e_i)
        return -1;
    }
    if (s_pos > e_pos && s_i == e_i)
      break;
  }
  return 0;
}
void* Mem_Alloc(int size) {
  
  if (size <= 0 || num_16 == 0 ) //num_16=0 -> no free 16B slots
    return NULL;

  if(size % 8 != 0)
		size += 8 - (size % 8);

  int num = 1; 
  if (size == 80) num = 5;
  if (size == 256) num = 16;
  int val = 1; 
  if (size == 80) val = 2;
  if (size == 256) val = 3;
 
  if (num_16 < num)
    return NULL;
  start_i = end_i = start_pos = end_pos = -1; 
  
  if (Find00Pos(bmask, num) != 0)
    return NULL;
  //printf("start_i = %d start_pos = %d end_i = %d end_pos = %d \n", start_i, start_pos, end_i, end_pos);
  
  SetVal(bmask, val);
  //printf("Allocating mem=%d ; pos = %d in bmask index %d. Free slot number = %d \n", size, pos, i, 32*i+pos);
  void *user = map + (16*start_i + start_pos/2)*16;
  num_16-=num;
  return user;
}

int Mem_Free(void* ptr) {
  
  if (ptr == NULL)
    return -1;
  if ((uintptr_t)ptr % 16 != 0) //every pointer returned by Mem_Alloc will be 16B aligned
    return -1;
   
  int slot = (ptr - map)/16;
  //printf("slot to be freed = %d \n", slot);
  start_i = slot/16;
  if(start_i >= arrl) return -1;
  start_pos = 2*(slot%16);
  
  int num;
  unsigned int val = *(bmask+start_i);
  val = (val & (3<<start_pos))>>start_pos;
  if(!val)
    return -1;

  if (val == 1) {
    end_pos = start_pos;
    end_i = start_i;
    num_16++;
  }
  else {
    num = (val==2)?5:16;
    num_16+=num;
    //need to calculate end_i and end_pos
    int i = start_i;
    end_i = start_i;
    int pos = start_pos+2;
    end_pos = pos;
    
    while(num > 1) {
      if (pos >= 32) {
        i++;
        if (i >= arrl)
          return -1;
        pos %= 32;
      }
      unsigned int v = (*(unsigned int*)(bmask+i) & (3<<pos))>>pos;
      if (v != val)
        return -1;
      num--;
      pos+=2;
    }
    end_pos = pos-2;
    end_i = i;
  }
  SetVal(bmask, 0);
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

