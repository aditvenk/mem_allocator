#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ALLIGNED 8
#define MAGIC	0xdeadbeef

typedef struct __node_t {
	int size;
	struct __node_t *next;
} node_t;

typedef struct __header_t {
	int size;
	int magic;
} header_t;

node_t* head_node=NULL;
node_t dummy_node;

int Mem_Init(int size)
{
	// Return failure if Mem_Init is called more than once
	if(head_node != NULL || size <=0)
		return -1;
	
  // Get page size
	int page_size = getpagesize();
	
	// Round up size
	if(size % page_size != 0) {
		size = size + (page_size - (size % page_size));
	}
  int fd = open("/dev/zero", O_RDWR);	
  // Get memory of size rounded_size
	head_node = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (head_node == MAP_FAILED) return -1; 
  
	if((uintptr_t)head_node  % 8 != 0)
	{
		head_node += 8 - ((uintptr_t)head_node % 8);
		size -= 8 - ((uintptr_t)head_node % 8);
	}

	
	head_node->size = size;
	head_node->next = NULL;
	dummy_node.size = -1;
	dummy_node.next = head_node;	
  close(fd);
	return 0;	
}

void* Mem_Alloc(int size)
{

	if(size <= 0 || head_node == NULL || dummy_node.next == NULL) 
		return NULL;
  
  // Round user-requested size to 8-aligned
	if(size % ALLIGNED != 0) {
		size = size + (ALLIGNED - (size % ALLIGNED));
	}
  
  node_t* big_node = head_node; //biggest node
  node_t* big_node_p = &dummy_node;
  node_t* big_node_n = head_node->next;
  node_t* cur_node = &dummy_node;
	int done = 0;
	
	int req_size = size + sizeof(header_t);
	
  while (cur_node->next != NULL) {
    if((cur_node->next)->size > big_node->size) {
      big_node = cur_node->next;
      big_node_p = cur_node;
      big_node_n = (cur_node->next)->next;
    }
    cur_node = cur_node->next;
  }
  int big_node_size = big_node->size; 
  
  if (big_node_size < req_size)
    return NULL;
  if (big_node_size == req_size || big_node_size <= (req_size + sizeof(node_t))) {
    if(big_node_size != req_size)  // not enough space for a new free list node. 
      req_size = big_node_size;
    
    big_node_p->next = big_node_n;
    if (big_node == head_node) { 
      head_node = big_node_n;
      dummy_node.next = head_node;
    }
    
    header_t *header = (header_t*) big_node;
    header->size = req_size;
    header->magic = MAGIC;
    //printf("about to return %p to user \n", (void*)header+sizeof(header_t));
    return (void*)header + sizeof(header_t);
  }
  else { //split big_node
    header_t *header = (header_t*) big_node;
    header->size = req_size;
    header->magic = MAGIC;
    void* user = (void*) header + sizeof(header_t);
    node_t *new_node = (node_t*) ((void*)big_node + req_size);
    big_node_p->next = new_node;
    new_node->next = big_node_n;
    new_node->size = big_node_size - req_size;
    if(big_node == head_node) {
      head_node = new_node;
      dummy_node.next = head_node;
    }
    //printf("about to return %p to user. head_node=%p \n", user, head_node);
    return user;
  }
}

int Mem_Free(void* ptr)
{
  if(ptr == NULL) return -1;
  if ((uintptr_t)ptr % 8 != 0) 
    return -1;

  header_t *header = (header_t*)(ptr - sizeof(header_t));
  if (header->magic != MAGIC)
    return -1;
  
  node_t* new_node_p = head_node;
  node_t* new_node = (node_t*) header;
  
  if(head_node == NULL) {
    new_node->size = header->size;
    new_node->next = NULL;
    head_node = new_node;
    dummy_node.next = head_node;
  }
  else if ((void*) new_node < (void*) head_node) { //freed node is lower in address than head of free list
    new_node->size = header->size;
    new_node->next = head_node;
    head_node = new_node;
    dummy_node.next = head_node;
  }
  else {
    while((void*) new_node_p->next < (void*) new_node){
      if (new_node_p->next == NULL) 
        break;
      new_node_p = new_node_p->next;
    }
    new_node->size = header->size;
    new_node->next = new_node_p->next;
    new_node_p->next = new_node;
  
  } 
  //coalesce before
  if (new_node_p != NULL) {
    if ( (void*)new_node_p + new_node_p->size == (void*) new_node) {
      new_node_p->next = new_node->next;
      new_node_p->size += new_node->size;
      new_node = new_node_p;
    }
  }
  //coalesce after
  if (new_node->next != NULL) {
    if (((void*) new_node + new_node->size)== new_node->next) {
      new_node->size+=(new_node->next)->size;
      new_node->next = (new_node->next)->next;
    }
  }
  return 0;
}

int Mem_Available()
{
  //return total usable memory
  node_t* cur_node = head_node;
  int size = 0;
  while(cur_node != NULL) {
    size += (cur_node->size - sizeof(node_t));
    cur_node = cur_node->next;
  }
	return size;
}

void Mem_Dump()
{
  node_t* cur_node = head_node;
  while(cur_node != NULL) {
    printf("Free node: %p : size = %d  : next = %p \n", cur_node, cur_node->size, cur_node->next);
    cur_node = cur_node->next;
  }
  printf("\n");
}
