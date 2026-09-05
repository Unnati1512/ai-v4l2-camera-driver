# Architecture Overview

## Registration Chain
module_init -> v4l2_device_register -> video_device_alloc -> 
video_register_device (creates /dev/videoX) -> vb2_queue_init -> 
vdev->queue linked

## Capture Pipeline
App REQBUFS -> queue_setup (count/size negotiated)
App QBUF -> buffer_queue (added to spinlock-protected tracking array)
App STREAMON -> start_streaming (launches kthread)
Kthread loop -> pulls buffer, memset fill, vb2_buffer_done
App DQBUF -> receives completed buffer

## Concurrency Protection
- vdev->lock / vb2_q.lock (mutex): serializes ioctl calls
- buffer_lock (spinlock): protects the internal tracking array between 
  buffer_queue (app-triggered) and capture_thread_fn (independent thread)
