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
struct bucket {
    struct spinlock lock;
    struct buf head;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[13];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < 13; i++) {
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    bcache.buckets[i].head.prev = &bcache.buckets[i].head; 
    bcache.buckets[i].head.next = &bcache.buckets[i].head; 
  }
  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf* 
bget(uint dev, uint blockno)
{
  struct buf *b;
  int buck = (dev * blockno) % 13;
  acquire(&bcache.buckets[buck].lock);

  // Is the block already cached?
  for(b = bcache.buckets[buck].head.next; b != &bcache.buckets[buck].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[buck].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[buck].lock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf * bb = 0;
  int bucket_to_evict = -1;
  for(int i = 0; i < 13; i++) {
    acquire(&bcache.buckets[i].lock);
    for(b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev) {
        if(b->refcnt == 0) {
            break;
        }     
    }
    if(b == &bcache.buckets[i].head)  {
        release(&bcache.buckets[i].lock);
        continue;
    }
    if(bb == 0 || bb->tick < b->tick) {
        bb = b;
        if(bucket_to_evict != -1)
            release(&bcache.buckets[bucket_to_evict].lock);
        bucket_to_evict = i;
        continue;
    }
    else {
        release(&bcache.buckets[i].lock);
    }
  }
  if(!bb)
    panic("bget: no buffers");
  bb->next->prev = bb->prev;
  bb->prev->next = bb->next;
  release(&bcache.buckets[bucket_to_evict].lock);
  acquire(&bcache.lock);
  acquire(&bcache.buckets[buck].lock);
  for(b = bcache.buckets[buck].head.next; b != &bcache.buckets[buck].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      bb->prev = bcache.buckets[buck].head.prev;
      bb->next = &bcache.buckets[buck].head;
      bb->prev->next = bb;
      bb->next->prev = bb;
      release(&bcache.buckets[buck].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  bb->next = bcache.buckets[buck].head.next;
  bb->prev = &bcache.buckets[buck].head;
  bb->next->prev = bb;
  bb->prev->next = bb;
  bb->dev = dev;
  bb->blockno = blockno;
  bb->valid = 0;
  bb->refcnt = 1;
  release(&bcache.buckets[buck].lock);
  release(&bcache.lock);
  acquiresleep(&bb->lock);
  return bb;
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
//   printf("%d\n", b->blockno);
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

  int buck = (b->dev * b->blockno) % 13;
  acquire(&bcache.buckets[buck].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[buck].head.next;
    b->prev = &bcache.buckets[buck].head;
    bcache.buckets[buck].head.next->prev = b;
    bcache.buckets[buck].head.next = b;
    b->tick = ticks;
  }
  release(&bcache.buckets[buck].lock);
  
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


