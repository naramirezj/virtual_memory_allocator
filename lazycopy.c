#define _GNU_SOURCE
#include "lazycopy.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/**
 * This function will be called at startup so you can set up a signal handler.
 */
typedef struct node {
  struct node* next;
  void* ptr;
  // void* original;
  void* copy;

} node_t;

typedef struct list {
  struct node* first;

} list_t;

void list_insert(list_t* lst, void* value1, void* value2);
void alarm_handler(int signal, siginfo_t* info, void* ctx);
void list_init(list_t* lst);
void addNode(list_t* lst, node_t* node);
struct sigaction sa;
const int MAX_ARGS = 128;
int counterEager = 0;
int counterLazy = 0;
// char* arr_chunk[MAX_ARGS];
/*
__attribute__((constructor)) void init() {
}
*/
/*
1. Need Alarms to go off
2. Will need an alarm Handler
3. Use sigaction to identify the sig fault
4.
*/

list_t* list_chunks;

void chunk_startup() {
  // TODO: Implement this function
  // init();
  list_chunks = malloc(sizeof(list_t));
  list_init(list_chunks);
  printf("This code runs at program startup\n");

  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_sigaction = alarm_handler;
  sa.sa_flags = SA_SIGINFO;

  if (sigaction(SIGSEGV, &sa, NULL) != 0) {
    perror("sigaction failed");
    exit(2);
  }
}

/**
 * This function should return a new chunk of memory for use.
 *
 * \returns a pointer to the beginning of a 64KB chunk of memory that can be read, written, and
 * copied
 */
void* chunk_alloc() {
  // Call mmap to request a new chunk of memory. See comments below for description of arguments.
  void* result = mmap(NULL, CHUNKSIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
  // Arguments:
  //   NULL: this is the address we'd like to map at. By passing null, we're asking the OS to
  //   decide. CHUNKSIZE: This is the size of the new mapping in bytes. PROT_READ | PROT_WRITE:
  //   This makes the new reading readable and writable MAP_ANONYMOUS | MAP_SHARED: This mapes a
  //   new mapping to cleared memory instead of a file,
  //                               which is another use for mmap. MAP_SHARED makes it possible for
  //                               us to create shared mappings to the same memory.
  //   -1: We're not connecting this memory to a file, so we pass -1 here.
  //   0: This doesn't matter. It would be the offset into a file, but we aren't using one.

  // Check for an error
  if (result == MAP_FAILED) {
    perror("mmap failed in chunk_alloc");
    exit(2);
  }

  // Everything is okay. Return the pointer.
  return result;
}

/**
 * Create a copy of a chunk by copying values eagerly.
 *
 * \param chunk This parameter points to the beginning of a chunk returned from chunk_alloc()
 * \returns a pointer to the beginning of a new chunk that holds a copy of the values from
 *   the original chunk.
 */

void* chunk_copy_eager(void* chunk) {
  // First, we'll allocate a new chunk to copy to
  void* new_chunk = chunk_alloc();

  // Now copy the data
  memcpy(new_chunk, chunk, CHUNKSIZE);
  counterEager++;
  // Return the new chunk
  return new_chunk;
}

/**
 * Create a copy of a chunk by copying values lazily.
 *
 * \param chunk This parameter points to the beginning of a chunk returned from chunk_alloc()
 * \returns a pointer to the beginning of a new chunk that holds a copy of the values from
 *   the original chunk.
 */

void* chunk_copy_lazy(void* chunk) {
  // Just to make sure your code works, this implementation currently calls the eager copy version
  // return chunk_copy_eager(chunk);
  // Your implementation should do the following:
  // void* chunk2 = chunk_alloc();
  // 1. Use mremap to create a duplicate mapping of the chunk passed in
  void* chunk2 = mremap(chunk, 0, CHUNKSIZE, MREMAP_MAYMOVE);
  if (chunk2 == MAP_FAILED) {
    perror("mremap failed");
    exit(EXIT_FAILURE);
  }
  counterLazy++;
  // 2. Mark both mappings as read-only
  if (mprotect(chunk, CHUNKSIZE, PROT_READ) != 0) {
    perror("mprotect Failed");
  }
  if (mprotect(chunk2, CHUNKSIZE, PROT_READ) != 0) {
    perror("mprotect Failed");
  }

  // 3. Keep some record of both lazy copies so you can make them writable later.
  list_insert(list_chunks, chunk, chunk2);
  // list_insert(list_chunks, chunk2, );

  return chunk2;
}

void alarm_handler(int signal, siginfo_t* info, void* ctx) {
  node_t* curr_node = list_chunks->first;
  bool found = false;

  while (curr_node != NULL) {
    intptr_t currChunk = (intptr_t)curr_node->ptr;
    intptr_t ptrChunk = (intptr_t)info->si_addr;
    if ((ptrChunk >= currChunk) && (ptrChunk <= currChunk + CHUNKSIZE)) {
      // 1. Save the contents of the chunk elsewhere (a local array works well)
      // void* storedChunk = (void*)currChunk;
      // 2. Use mmap to make a writable mapping at the location of the chunk that was written
      mmap((void*)currChunk, CHUNKSIZE, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_SHARED | MAP_FIXED, -1, 0);
      // 3. Restore the contents of the chunk to the new writable mapping
      memcpy(curr_node->ptr, curr_node->copy, CHUNKSIZE);
      // printf("Mapping %ld, with %p\n", currChunk, storedChunk);
      // printf("Hey\n");
      found = true;
      break;
    }
    curr_node = curr_node->next;
  }
  if (!found) {
    char* str[8] = {"Try again!",          "You got this!",           "Almost There",
                    "Tuff luck",           "give it another go!",     "Seg Error, sucks",
                    "check your pointers", "Go to the mentor session"};

    srand(time(NULL));
    int random = rand() % 8;

    printf("%s\n", str[random]);
  }

  // printf("eager #: %d, Lazy #: %d\n", counterEager, counterLazy);
}

void list_insert(list_t* lst, void* value1, void* value2) {
  node_t* original_node = (node_t*)malloc(sizeof(node_t));
  node_t* copy_node = (node_t*)malloc(sizeof(node_t));

  original_node->ptr = value1;
  copy_node->ptr = value2;
  original_node->copy = copy_node->ptr;
  copy_node->copy = original_node->ptr;
  original_node->next = NULL;
  copy_node->next = NULL;

  addNode(lst, original_node);
  addNode(lst, copy_node);
}

void addNode(list_t* lst, node_t* node) {
  if (lst->first == NULL) {
    node->next = lst->first;
    lst->first = node;
  } else {
    node_t* curr_node = lst->first;
    while (curr_node->next != NULL) {
      curr_node = curr_node->next;
    }
    // node->next = curr_node->next;
    curr_node->next = node;
  }
}

void list_init(list_t* lst) {
  lst->first = NULL;
}
