// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

extern uint ticks;

struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
  struct spinlock head_lock[NBUCKET];
} bcache;

char * head_lock_name[] = {
  "bcache_head_lock_0",
  "bcache_head_lock_1",
  "bcache_head_lock_2",
  "bcache_head_lock_3",
  "bcache_head_lock_4",
  "bcache_head_lock_5",
  "bcache_head_lock_6",
  "bcache_head_lock_7",
  "bcache_head_lock_8",
  "bcache_head_lock_9",
  "bcache_head_lock_10",
  "bcache_head_lock_11",
  "bcache_head_lock_12",
};

void
binit(void)
{
  struct buf *b;

  // Create linked list of buffers
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.head_lock[i], head_lock_name[i]);
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for (int i = 0; i < NBUF; i++) {
    uint hash_idx = i % NBUCKET;
    b = &bcache.buf[i];

    b->next = bcache.head[hash_idx].next;
    b->prev = &bcache.head[hash_idx];
    b->last_use = ticks;
    initsleeplock(&bcache.buf[i].lock, "buffer");
    bcache.head[hash_idx].next->prev = b;
    bcache.head[hash_idx].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint min_last_use;

  uint hash_idx = blockno % NBUCKET;

  acquire(&bcache.head_lock[hash_idx]);

  // Is the block already cached?
  for(b = bcache.head[hash_idx].next; b != &bcache.head[hash_idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->last_use = ticks;
      release(&bcache.head_lock[hash_idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used unused buffer.
  min_last_use = __UINT32_MAX__;
  b = 0;
  for (int i = 0; i < NBUF; i++) {
    if (bcache.buf[i].refcnt == 0 && min_last_use > bcache.buf[i].last_use) {
      min_last_use = bcache.buf[i].last_use;
      b = &bcache.buf[i];
    }
  }

  if (b != 0) {
    // Find an available buffer
    uint no = b->blockno;
    if (no % NBUCKET == hash_idx) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->last_use = ticks;
      release(&bcache.head_lock[hash_idx]);
      acquiresleep(&b->lock);
      return b;
    }
    acquire(&bcache.head_lock[no % NBUCKET]);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->last_use = ticks;
    // Steal!
    b->next->prev = b->prev;
    b->prev->next = b->next;
    release(&bcache.head_lock[no % NBUCKET]);

    b->next = bcache.head[hash_idx].next;
    b->prev = &bcache.head[hash_idx];
    bcache.head[hash_idx].next->prev = b;
    bcache.head[hash_idx].next = b;
    release(&bcache.head_lock[hash_idx]);
    acquiresleep(&b->lock);
    return b;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hash_idx = b->blockno % NBUCKET;
  acquire(&bcache.head_lock[hash_idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->last_use = ticks;
  }
  release(&bcache.head_lock[hash_idx]);
}

void
bpin(struct buf *b) {
  uint hash_idx = b->blockno % NBUCKET;
  acquire(&bcache.head_lock[hash_idx]);
  b->refcnt++;
  release(&bcache.head_lock[hash_idx]);
}

void
bunpin(struct buf *b) {
  uint hash_idx = b->blockno % NBUCKET;
  acquire(&bcache.head_lock[hash_idx]);
  b->refcnt--;
  release(&bcache.head_lock[hash_idx]);
}


