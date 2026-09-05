# Week 2 Summary — The Full Capture Path, Buffer State by Buffer State

## What happens, in order, when an app streams from my driver?

When an app calls REQBUFS, it reaches my queue_setup function, which 
decides how many buffers to create (minimum 2, enforced in code) and 
how large each one needs to be (614,400 bytes, based on 640x480 YUYV). 
vb2 then actually allocates that memory using the vmalloc strategy I 
configured. QUERYBUF lets the app inspect each allocated buffer's 
metadata before using it. When the app calls QBUF to hand a buffer over 
to my driver, my buffer_queue function adds it to a spinlock-protected 
internal tracking array - this is the handoff point where the buffer 
becomes "mine" to fill. STREAMON then launches my kernel thread 
(kthread), which runs continuously in the background: it pulls the 
oldest buffer from that tracked list, fills it with synthetic pixel 
data via memset, marks it done through vb2_buffer_done, and repeats 
this roughly every 33ms. Finally, DQBUF returns a completed buffer back 
to the app, which can read the real bytes I wrote into it.

## What's the single hardest concept from this week?

Understanding that vb2_queue_init has multiple genuinely required 
fields (a working buf_queue callback, a properly initialized lock) that 
aren't fully obvious from documentation alone - some only surfaced as 
runtime validation failures specific to my exact kernel version. I hit 
two separate -EINVAL failures on Day 6 before getting this right, and 
the second one (the missing .lock field) only made sense once I 
connected it back to Day 5's locking concepts - vb2 needs a lock to 
safely serialize buffer-queue operations, the same reasoning behind 
my own spinlock protecting the tracking array in Day 11.

## What did the robustness testing (Day 13) actually prove?

That correctness under normal use isn't the same as correctness under 
concurrent or abnormal use. Double-open testing proved my locking 
actually prevents two sessions from corrupting the queue simultaneously, 
not just in theory. Abrupt-disconnect testing proved my cleanup path 
runs even when an app doesn't shut down gracefully. And rmmod-while-
streaming proved something I hadn't personally written any code for: 
the kernel's own module reference counting refuses to let in-use driver 
code be removed from memory, an independent safety layer sitting 
underneath my own mutex and spinlock. Testing these deliberately, 
before they could happen accidentally in front of someone else, is 
exactly why this day existed on the roadmap.

## What am I building toward in Week 3?

A userspace AI inference application that reads real frames out of this 
exact pipeline - opening /dev/video0, requesting and mapping buffers the 
same way v4l2-ctl does, but instead of just counting or saving frames, 
running a lightweight object-detection model on each one in real time. 
Week 1 and 2 built the mechanism that gets a frame from my kernel thread 
into an app's hands; Week 3 is about proving that mechanism is fast and 
reliable enough to actually feed a real-time AI pipeline, which is the 
entire "AI-ready" premise of this project.
