
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

int reference_counter_matrix[PHYSTOP / PGSIZE];


void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}


void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    reference_counter_matrix[(uint64)p / PGSIZE] = 1; 
    kfree(p);
  }
}
//Increase reference counter
void inc_ref_counter(uint64 pa, int i){
  acquire(&kmem.lock);
  int id = pa / PGSIZE;
  reference_counter_matrix[id]+=i;
  release(&kmem.lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  r = (struct run *)pa;
  int id = (uint64)r / PGSIZE;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);

  if (reference_counter_matrix[id] < 1)
    panic("kfree");
  
  reference_counter_matrix[id] -= 1;

  if (reference_counter_matrix[id]==0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  int id = (uint64)r / PGSIZE;
  if(r){
    reference_counter_matrix[id]=1; //  Όταν ανατίθεται αρχικά μία σελίδα μέσω της kalloc(), ο μετρητής αναφορών αρχικοποιείται στην τιμή 1.
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}