# Locking Concepts — My Notes (Day 5)

## What problem does locking solve?
A race condition happens when two pieces of code try to touch the 
same shared data at the same time, with no coordination. For example: 
a background timer filling a camera frame buffer at the exact same 
moment an app is trying to read from that same buffer. If nothing 
stops this, the app might read half-old, half-new data - a corrupted 
frame. Worse, this kind of bug doesn't happen every time, only when 
the timing lines up badly, which makes it genuinely hard to catch and 
debug. A lock is simply a rule that says "only one of these two 
things is allowed to touch this data at once - the other has to 
wait its turn."

## Spinlock vs Mutex — when would I use each?
Both are ways of making something WAIT its turn for a lock, just in 
different styles:
- A SPINLOCK waits by actively checking, over and over, as fast as 
  possible, until the lock is free ("is it free yet? is it free 
  yet?"). This burns CPU cycles while waiting, but is the right 
  choice when the wait is expected to be very short - checking 
  constantly for a few microseconds is cheaper than the overhead of 
  actually going to sleep and being woken up later. Critical rule: 
  a spinlock must never be held across any code that might sleep, 
  because if the lock-holder goes to sleep while others are actively 
  spinning waiting for it, that's a deadlock.
- A MUTEX waits by actually going to sleep, releasing the CPU 
  entirely, and getting woken up later when the lock becomes free. 
  This is the right choice for longer operations, or anywhere the 
  code might need to sleep anyway (like waiting on hardware or user 
  input) - since burning CPU cycles constantly checking would be 
  wasteful for a longer wait.

Simple rule I'm using to remember this: short and fast -> spinlock. 
Longer, or already involves sleeping -> mutex.

## Where will I actually need this in my driver?
Starting Week 2, my vb2 buffer queue will be touched by more than one 
thing at once for the first time: a background kernel timer/thread 
filling buffers with synthetic frame data, while an application is 
simultaneously trying to read/dequeue a buffer through my driver's 
ioctl handlers. That shared buffer queue is exactly the kind of data 
structure that needs protection - most likely a spinlock, since 
buffer-queue operations (checking/updating buffer state) tend to be 
short, fast operations rather than something that needs to sleep. 
The vb2_queue_init() call I already made on Day 6 actually sets up 
some of this locking internally on my behalf, but understanding WHY 
it's needed helps me understand what that call is actually doing 
under the hood, and will matter directly once I write my own 
streaming logic in Week 2.
