
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
