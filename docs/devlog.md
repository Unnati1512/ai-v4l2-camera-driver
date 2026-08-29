
## Day 4 — V4L2 Device Registration 
Registered my module as a real V4L2 device - v4l2_device and 
video_device structures, VIDIOC_QUERYCAP implemented and verified 
working with the real v4l2-ctl tool. Hit two genuine bugs along the 
way: v4l2_dev.name must be set before v4l2_device_register() when 
passing NULL as parent device, and file operations must use V4L2's own 
helper functions (v4l2_fh_open/v4l2_fh_release) rather than plain 
return 0 - using bare stubs left the driver in an inconsistent state 
that caused intermittent "No such device" errors. Added printk debug 
statements throughout init/open/release to make the actual execution 
path visible in dmesg, which was the key to finally diagnosing this 
properly instead of guessing from outside.


## Day 5 — Robustness Verification & Locking Concept
Verified Day 4's driver under repeated stress (5x open/query cycles 
via v4l2-ctl) with no warnings or crashes in the kernel log. Also 
checked kernel taint status - initially confused an unrelated taint 
flag (caused by VirtualBox's own guest driver, vboxguest, which 
taints the kernel on every boot regardless of my module) as a 
possible bug in my own code before realizing it was unrelated 
background noise. Studied locking concepts (spinlocks vs mutexes) 
ahead of Week 2, where my buffer queue will be accessed by multiple 
things at once for the first time.

**Lesson:** not every unusual system signal (like a nonzero taint 
value) is actually about my own code - worth checking what's 
genuinely caused by my module versus pre-existing system state before 
treating something as a new bug.

## Day 6 — vb2 Queue Scaffolding 
Added a vb2_queue to my driver with queue_setup, buf_prepare, and 
buf_queue callback functions, and got vb2_queue_init to succeed. Went 
further than the roadmap's minimum bar (just "compiles and is wired 
up") - got it fully, functionally initializing after real debugging.

**Bugs hit, root cause, fix:**
1. vb2_queue_init failed with -EINVAL because my vb2_ops struct was 
   missing a .buf_queue callback - vb2_core_queue_init requires this 
   specific callback to be non-null. Fixed by adding a buffer_queue 
   function and wiring it in.
2. Still failed with -EINVAL after that fix. Root cause: vb2_queue 
   also requires a `.lock` field (a mutex) assigned, for internal 
   serialization of buffer-queue ioctl operations - this connects 
   directly to Day 5's locking concepts. Fixed by assigning the same 
   mutex already created for the video_device: 
   `vb2_q.lock = &myv4l2_lock;`

I went through some incorrect guesses (min_queued_buffers, 
max_num_buffers) from kernel source references before landing on the 
real fix - found the actual missing field myself by reasoning about 
what resource was already available in the code.

**Key concept:** vb2_q.lock isn't a workaround - it's the real 
mechanism vb2 uses to safely serialize buffer operations, directly 
relevant once REQBUFS/QBUF/DQBUF handle real concurrent access 
starting Day 10.

## Day 8 — Real queue_setup and buffer_prepare Logic 
Replaced Day 6's placeholder queue_setup/buffer_prepare functions with 
real logic. queue_setup now negotiates actual buffer count (enforces 
a minimum of 2, since streaming needs at least one buffer available 
to fill while another is being read) and reports the exact byte size 
needed per buffer, based on a fixed 640x480 frame. buffer_prepare 
validates an allocated buffer is actually large enough, then reports 
the real "payload" size (how much of the buffer contains meaningful 
data) via vb2_set_plane_payload.

Verified clean load, registration, v4l2-ctl --info and --list-devices 
both working, clean unload.

**Worth noting (not a bug):** also tried 
`v4l2-ctl --stream-mmap --stream-count=1`, which failed with 
"VIDIOC_REQBUFS returned -1 (Inappropriate ioctl for device)". This 
is expected - my ioctl_ops struct only has .vidioc_querycap wired up 
so far; REQBUFS/QBUF/DQBUF get added Day 10, STREAMON on Day 11. This 
confirms my driver correctly reports capabilities it doesn't have yet, 
rather than indicating something broken.

**Key concept:** the buffer size/count negotiated today becomes the 
literal contract Day 11's frame-generator thread has to honor when 
actually writing pixel data into these buffers.

## Day 9 — Pixel Format Negotiation 
Implemented VIDIOC_ENUM_FMT, VIDIOC_G_FMT, VIDIOC_S_FMT, and 
VIDIOC_TRY_FMT for a single fixed format: YUYV 4:2:2 at 640x480. 
Chose YUYV specifically because it's the simplest, most universally 
supported V4L2 pixel format - the standard first-format choice for 
driver development (used by reference drivers like vivid for the same 
reason).

**What each ioctl actually does:**
- ENUM_FMT: lets an app discover which formats I support, one at a 
  time by index, until I return an error signaling "no more."
- TRY_FMT: lets an app ask "what would happen if I requested this 
  format" without committing - since I only support one fixed 
  format/resolution, this always returns my one true configuration.
- G_FMT/S_FMT: get/set the currently active format. S_FMT reuses 
  TRY_FMT's logic to compute values, then actually commits it to a 
  stored module-level variable.

**Key safety detail:** S_FMT checks `vb2_is_busy()` and refuses to 
change format if buffers have already been allocated (REQBUFS already 
called) - changing format after buffers exist, sized for the OLD 
format, would leave buffers that no longer match reality. This 
directly foreshadowed Day 10's REQBUFS work.

Verified with v4l2-ctl --list-formats-ext, --get-fmt-video, and 
--set-fmt-video - all correctly report/accept YUYV 640x480.

## Day 10 — REQBUFS/QUERYBUF/QBUF/DQBUF 
Wired up the actual buffer request/query/queue/dequeue ioctls using 
vb2's own ready-made functions (vb2_ioctl_reqbufs, vb2_ioctl_querybuf, 
vb2_ioctl_qbuf, vb2_ioctl_dqbuf) rather than writing this logic 
myself - vb2 provides standard implementations that plug directly 
into my ioctl_ops struct. Also added .mmap and .poll file operations 
(vb2_fop_mmap, vb2_fop_poll) so an app can actually memory-map the 
buffers vb2 allocates, and added V4L2_CAP_STREAMING to my declared 
capabilities, without which apps wouldn't know streaming is supported 
at all.

Verified progress by re-running the same stream test from Day 8: this 
time REQBUFS succeeded, and the failure point correctly moved forward 
to a STREAMON-related error instead of the earlier REQBUFS error - 
exactly the expected sign of progress, since STREAMON isn't 
implemented until Day 11.

**Key concept:** most of Week 2's "hard" ioctls (REQBUFS, QBUF, DQBUF) 
aren't actually written by the driver author from scratch - vb2 
provides correct, reusable implementations, and the real driver work 
is correctly configuring the queue (Days 6-9) so those shared 
functions have what they need to work against.
