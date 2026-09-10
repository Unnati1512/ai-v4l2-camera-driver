# DMA-BUF — My Notes (Day 22)

## What problem does DMA-BUF solve, that mmap (my current approach) doesn't?

So mmap is what I'm already using (Day 6 onwards) - it lets ONE 
userspace process map the buffer memory directly into its own address 
space and read/write it without a full copy_to_user style copy. That's 
already zero-copy in a sense, just limited to one process.

DMA-BUF goes a step further - it lets that same underlying memory be 
shared with something that ISN'T even a normal userspace process. The 
classic example is a GPU - if a camera driver hands off a frame via 
DMA-BUF, a GPU can directly read that exact same memory to do 
processing/rendering on it, without the CPU ever copying the data 
anywhere in between. The mechanism for this is a file descriptor (fd) - 
the driver exports the buffer as a dma-buf fd, and literally anything 
that understands dma-buf fds (another driver, a GPU, whatever) can 
import that fd and get access to the same physical memory.

Honestly the way I'm thinking about it: mmap = "let my app see this 
buffer directly." DMA-BUF = "let ANY compatible piece of hardware or 
driver see this buffer directly, not just my one app." It's the same 
zero-copy idea just applied one level higher up, between subsystems 
instead of just kernel-to-one-app.

## What's the attach/map/fence model?

This took me a couple rereads of the kernel doc to actually get.

- Attach: before a device can actually use a dma-buf, it has to 
  "attach" to it first. This is basically the exporter (my driver, in 
  theory) and the importer (whatever wants to use the buffer) agreeing 
  "ok we're going to share this." I think this step also lets the 
  exporter know WHO is attaching, which matters because different 
  hardware might have different constraints on what memory it can 
  actually access (like needing contiguous physical memory, which 
  is exactly my problem below).

- Map: once attached, the actual mapping step is what gives the 
  importer real, usable access to the memory - basically getting the 
  actual addresses it needs to read/write. Map/unmap can happen 
  multiple times after one attach, from what I read.

- Fence: this is the part I understand least well right now. From what 
  I can tell it's basically a synchronization primitive - since 
  multiple different pieces of hardware might be touching the same 
  buffer, you need SOME way to know "is this buffer currently being 
  written to by someone, or is it safe to read now." A fence is 
  basically a signal/promise that says "this operation is done" so 
  the next user doesn't read a half-finished buffer - kind of similar 
  in spirit to the locks I used in my own driver (Day 5/11), just for 
  coordinating across completely separate hardware/drivers instead of 
  within one driver.

## Why does my current vb2_vmalloc_memops choice make this harder?

This is the part where I think I'm going to hit a real wall.

I picked vb2_vmalloc_memops back on Day 6 specifically because my 
"camera" is synthetic - there's no real hardware doing DMA, my own 
kthread just writes fake pixel data with memset. vmalloc gives memory 
that's virtually contiguous but NOT necessarily physically contiguous 
(it can be made of scattered physical pages stitched together via page 
tables).

From what I've read, DMA-BUF export generally expects 
vb2_dma_contig_memops instead - physically contiguous memory - because 
the whole point of DMA-BUF is real hardware (like a GPU) reading the 
memory directly, and real hardware generally can't deal with the 
"scattered pages pretending to be one block" trick that vmalloc uses; 
it needs one real contiguous block of physical addresses.

So switching to dma-contig is probably a real requirement, not just an 
option - and dma-contig also apparently wants a real struct device 
representing actual hardware (for DMA addressing purposes), which my 
driver has never had, on purpose, since v4l2_device_register has been 
called with NULL as the parent this whole project. I have a feeling 
this is going to be the actual blocker tomorrow, not writing the 
EXPBUF ioctl itself - the ioctl part seems more like plumbing 
(vb2_ioctl_expbuf already exists), the real problem is probably going 
to be getting a valid contiguous allocation to happen at all without 
real hardware backing it. Guess I'll find out tomorrow.
