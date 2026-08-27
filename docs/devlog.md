
## Day 4 — V4L2 Device Registration (Aug 27, 2026)
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



## Day 5 — Robustness Verification & Locking Concepts (Aug 27, 2026)
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
