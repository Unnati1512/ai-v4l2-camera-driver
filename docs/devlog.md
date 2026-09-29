
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

**Verification detail:** confirmed REQBUFS actually succeeded (not 
just "didn't error") by checking that queue_setup's log line printed 
with the correct buffer count and size (4 buffers of 614400 bytes, 
matching Day 9's exact frame size calculation) - queue_setup only 
gets called by vb2_ioctl_reqbufs on successful buffer allocation, so 
this is stronger proof than just "no error was shown."

**Note on VIDIOC_CREATE_BUFS error:** v4l2-ctl also tried a newer 
optional ioctl (CREATE_BUFS) before falling back to REQBUFS - this 
failed because I didn't wire up .vidioc_create_bufs, which is correct 
and expected, since CREATE_BUFS isn't part of the required set for 
Day 10 (REQBUFS/QUERYBUF/QBUF/DQBUF only). v4l2-ctl's fallback to 
REQBUFS worked exactly as it should.

**Key concept:** most of Week 2's "hard" ioctls (REQBUFS, QBUF, DQBUF) 
aren't actually written by the driver author from scratch - vb2 
provides correct, reusable implementations, and the real driver work 
is correctly configuring the queue (Days 6-9) so those shared 
functions have what they need to work against.

## Day 11 — STREAMON/STREAMOFF with Synthetic Frame Generator 
Implemented the full streaming lifecycle: a spinlock-protected buffer 
queue (connecting directly to Day 5's locking concepts - protects a 
shared array touched by both buf_queue, called when apps queue 
buffers, and my capture thread, running independently), a kernel 
thread (kthread) that runs on a ~30fps loop filling buffers with 
synthetic pixel data, and start_streaming/stop_streaming vb2_ops 
callbacks that launch/stop this thread.

**Verified with full success**, using v4l2-ctl's --verbose output to 
see every ioctl in the chain succeed in order: REQBUFS -> QUERYBUF x4 
-> QBUF x4 -> STREAMON -> 5 real frames dequeued, each reporting 
bytesused: 614400 (exactly matching my frame size calculation from 
Day 8/9). This is genuine proof that data flows correctly from my 
kernel thread's memset() call, through vb2's buffer management, all 
the way to a real userspace tool - the actual payoff of everything 
built since Day 4.

**Key concept:** buffer_queue() adds a buffer to my own tracking 
array (protected by the spinlock) when an app queues it; the capture 
thread independently pulls from that same array, fills it, and calls 
vb2_buffer_done() to hand it back to vb2/the app. This producer-
consumer pattern, with the spinlock preventing the two sides from 
corrupting the shared array, is a direct real application of Day 5's 
locking theory - not just something I read about, something I 
actually needed and used.

## Day 12 — First Real Milestone: Saved Frames + Verified Output
Captured 10 real frames from my driver to an actual file on disk 
(v4l2-ctl --stream-to=captured_frames.raw), rather than just counting 
successful captures. Verified the file size independently against my 
own math: 10 frames x 614400 bytes/frame = 6,144,000 bytes (~5.9MB), 
which matched the actual file size exactly - real, external proof the 
byte count was correct, not just "no error was shown."

Wrote my first Python script in this project (using numpy + Pillow) 
to decode a raw YUYV frame into a viewable PNG image - extracted just 
the Y (luminance) channel for a simple grayscale preview. This is a 
direct precursor to Week 3's AI inference pipeline, which will do 
similar raw-frame decoding before running a model on it.
**Verification detail:** frame 0 rendered as solid black (fill_value 
starts at 0 - a static u8 variable with no explicit initializer 
defaults to 0 in C), while frame 4 rendered as a slightly lighter 
shade, matching the expected fill_value = 4 x 10 = 40. This confirmed 
my capture thread's shifting fill value is genuinely changing frame 
to frame, not stuck at one value - real evidence the frame generator 
logic works correctly across multiple frames, not just once.

**Milestone reached:** matches the roadmap's "first real milestone, 
don't rush this one" - genuine, file-verified, independently-checked 
proof of a working end-to-end capture pipeline.

## Day 13 — Robustness Testing: Concurrent Access & Safe Removal

**Goal:** verify my driver behaves safely under conditions beyond the 
normal happy path - concurrent access attempts, abrupt disconnects, and 
attempts to remove the module while it's actively in use.

**What I tested and the results:**

1. **Double-open (two apps requesting buffers simultaneously)** - ran 
   one v4l2-ctl stream in the background, then immediately tried to 
   open the same device with a second instance. The second request was 
   correctly rejected with "VIDIOC_REQBUFS returned -1 (Device or 
   resource busy)". This confirms vdev->lock (my mutex) and V4L2's 
   per-queue ownership model correctly prevent two sessions from 
   touching the buffer queue at once - no crash, no corrupted state.

2. **Abrupt disconnect (Ctrl+C mid-stream)** - killed a running stream 
   command mid-capture instead of letting it finish normally. dmesg 
   confirmed stop_streaming and capture thread stopping still ran 
   correctly, meaning my cleanup path handles a non-graceful client 
   disconnect, not just a clean STREAMOFF.

3. **rmmod while actively streaming** - attempted to unload the module 
   while a capture was in progress. Got "rmmod: ERROR: Module myv4l2 is 
   in use" - the kernel's own built-in module reference counting 
   refused the removal. This is a genuinely important finding: it's 
   not my own code's protection catching this, it's Linux's underlying 
   module system itself refusing to let in-use code be removed from 
   memory. Once streaming stopped, a follow-up rmmod succeeded cleanly 
   (confirmed via dmesg: init -> registered -> unregistered, no warnings).

**Where I actually got stuck, and how I solved it:**

- A stray space in a `--set-fmt-video` argument caused v4l2-ctl to dump 
  its entire help page instead of a clear error - fixed by removing the 
  space; a reminder that CLI argument parsing can fail in confusing, 
  non-obvious ways.
- Backgrounding a long stream command with `&` meant Ctrl+C had no 
  effect (it only signals the foreground process) - I had to learn and 
  use `jobs` to list background processes and `kill -9 %1` to actually 
  terminate the stuck one.
- The VM itself became unresponsive twice during testing - running 
  multiple overlapping long-duration (1000-10000 frame) streams on a 
  resource-constrained VM (2-3GB RAM, 2 CPUs) pushed it past what it 
  could reliably handle. I recovered both times with a clean VM reset 
  (Machine -> Reset in VirtualBox) rather than fighting a frozen SSH 
  session, then verified recovery with `uptime` and `lsmod` before 
  continuing.

**Not completed today:** a planned 50x rapid open/close/stream stress 
loop - the VM instability interrupted testing before I reached this 
step. Worth revisiting with more conservative stream counts.

**Commands that actually got me through today:**

jobs # list background jobs
kill -9 %1 # force-kill a stuck background job
sudo dmesg -T | tail # timestamped kernel log, easier to correlate
# with when things actually happened


**My actual takeaway from today:** robustness testing isn't just about 
proving my driver's own code is correct - it's also about learning the 
real limits of my test environment, and not confusing "the VM is 
overloaded" with "my driver has a bug." The rmmod-while-streaming result 
specifically was worth the trouble: it's genuine, verifiable proof that 
even if I'd missed something in my own locking, the kernel's own module 
system has an independent safety net underneath it.


## Day 15 — AI Inference Pipeline: ONNX Runtime Setup

Started Week 3 by getting a real inference pipeline running against my 
driver's output, rather than jumping straight into a live capture loop. 
Installed ONNX Runtime and OpenCV (headless build, since this VM has no 
display server) and picked MobileNetV2 as my first model - it's a small, 
widely-used image classification network (~14MB, ImageNet-trained, 1000 
output classes), a reasonable first target before moving to something 
heavier like object detection.

**GPU note:** running CPU-only inference here. VirtualBox doesn't expose 
GPU passthrough by default, and setting that up is its own separate 
rabbit hole I'm deliberately not chasing this week - CPU inference is 
sufficient to prove the pipeline itself works correctly, which is 
today's actual goal.

**What I verified, and why each step mattered:**
- Loaded the model and printed its expected input/output shapes 
  (input: [batch, 3, 224, 224], output: [batch, 1000]) before writing 
  any inference code - wanted to confirm exactly what shape of data the 
  model expects rather than guessing and debugging a shape mismatch 
  later.
- Ran inference on frame4_preview.png (my Day 12 synthetic capture) and 
  got a real, structured output - a predicted class index and 
  confidence score. Since that image is a flat gray frame with no real 
  object in it, the prediction itself is meaningless - that's expected 
  and fine, since the point today was proving the full chain (load 
  model -> preprocess image -> run inference -> get output) executes 
  without errors, not that the prediction is correct.
- Wanted a way to actually confirm the model was processing real pixel 
  data rather than silently returning some fixed/cached result 
  regardless of input, so I generated a second, visually distinct test 
  image (a synthetic checkerboard pattern) and reran inference against 
  it. Getting a different class index/confidence than the gray-frame 
  test confirms the model is genuinely reading and reacting to the 
  actual input array, not just returning a constant.

**Key concept:** preprocessing has to match exactly what the model 
expects - resizing to 224x224, normalizing pixel values to a 0-1 float 
range, and reordering the array axes from (height, width, channels) to 
(channels, height, width) to match PyTorch/ONNX's expected tensor 
layout. Getting any of these wrong wouldn't necessarily error out - it 
would just silently produce garbage predictions, which is why the 
checkerboard sanity check felt worth doing before trusting this 
pipeline going forward.

**What's next:** this static-image test proves the inference side works. 
The actual integration - reading live frames directly out of my V4L2 
driver's buffers instead of a saved PNG - starts next.

## Day 16 — Object Detection with Bounding Boxes: YOLOv8n

Today I moved from image classification to real object detection using
YOLOv8n. Unlike the classification model used previously, an object
detection model should identify both *what* an object is and *where* it
is in the image by producing bounding boxes.

### 1. First approach: exporting YOLOv8n to ONNX

My initial plan was to install the `ultralytics` package and export a
YOLOv8n model to ONNX myself.

However, installing `ultralytics` pulled in PyTorch, torchvision and a
large CUDA-related dependency stack. The PyTorch download alone was
around 554 MB. The installation eventually failed with a disk quota
error.

This was confusing because `df -h ~` showed approximately 17 GB of
actual free disk space.

The important distinction was that the error was not caused by the
Ubuntu filesystem being full. It was a separate per-user disk quota
being exceeded by the large package installation.

### Solution

Since the goal of this day was to understand and implement the
userspace object-detection pipeline rather than to study model export,
I avoided installing the entire Ultralytics/PyTorch toolchain.

Instead, I downloaded an already-converted YOLOv8n ONNX model directly
from a public model host.

The resulting `yolov8n.onnx` file was approximately 13 MB.

This allowed me to continue with ONNX Runtime without pulling a large
deep-learning framework and CUDA dependencies into the VM.

### 2. Verified the ONNX model

I inspected the downloaded model before writing the detection code.

The model reported:

- Input name: `images`
- Input shape: `[1, 3, 640, 640]`
- Output name: `output0`
- Output shape: `[1, 84, 8400]`

This confirmed that the ONNX model was loading correctly and that I
could proceed with preprocessing, inference and output parsing.

### 3. First detection attempt — boxes were invisible

The first version of `detect_and_draw.py` ran successfully and reported:

    Detections drawn: 43
    Saved detected_output.png

However, the generated image did not visibly contain bounding boxes.

At first this was confusing because the program was not producing any
Python errors and it was apparently detecting objects.

I therefore added debug output to inspect the raw bounding-box values
before converting them into image coordinates.

The output included values such as:

    cx=0.4968 cy=0.4543 w=0.9873 h=0.4861
    class_id=5 conf=0.76

The values were clearly in the normalized 0-1 range.

For example, the detected bus had:

    cx ≈ 0.50
    cy ≈ 0.45
    w  ≈ 0.99
    h  ≈ 0.49

These values made sense for the test image because the bus occupies a
large portion of the frame.

### 4. Root cause — incorrect coordinate conversion

The problem was in my conversion from YOLO's normalized coordinates
to the original image coordinates.

I originally used:

    x1 = (cx - w / 2) / INPUT_SIZE * orig_w
    y1 = (cy - h / 2) / INPUT_SIZE * orig_h
    x2 = (cx + w / 2) / INPUT_SIZE * orig_w
    y2 = (cy + h / 2) / INPUT_SIZE * orig_h

The mistake was dividing by `INPUT_SIZE` (640).

The debug output showed that the coordinates I was working with were
already normalized fractions of the image, approximately in the
range 0-1. Dividing them by 640 again reduced them to values close to
zero.

As a result, the calculated bounding boxes were effectively only about
1 pixel in size and were drawn near the top-left corner of the image.

This explained why the program could report successful detections while
the output image appeared to contain no boxes.

### 5. Fix

I changed the conversion to multiply the normalized coordinates
directly by the original image dimensions:

    x1 = (cx - w / 2) * orig_w
    y1 = (cy - h / 2) * orig_h
    x2 = (cx + w / 2) * orig_w
    y2 = (cy + h / 2) * orig_h

No additional division by 640 is required.

For example, a normalized box with approximately:

    cx = 0.497
    cy = 0.454
    w  = 0.987
    h  = 0.486

corresponds to a box covering most of the image width, which matches
the position and size of the bus in the test photograph.

### 6. Result after the fix

After correcting the coordinate conversion, the bounding boxes became
visible in the generated image.

The output now correctly shows bounding boxes around the detected bus
and pedestrians in the test photograph.

The detection pipeline is therefore working end-to-end:

    image -> resize to 640*640 -> normalize / rearrange channels -> YOLOv8n ONNX inference ->  parse [1, 84, 8400] output -> obtain class + confidence + bounding box ->  convert normalized coordinates to original image coordinates -> draw bounding boxes ->  save detected_output.png

### 7. Current limitation — duplicate boxes

The current implementation still draws multiple overlapping boxes for
the same physical object.

For example, the model produced several high-confidence predictions
for the same bus. This is why the final image contains several
overlapping boxes instead of one clean box per object.

This is expected with the current implementation because Non-Maximum
Suppression (NMS) has not been implemented yet.

NMS will be added as a post-processing step so that overlapping
predictions referring to the same object can be reduced to the best
detection.

### Key lessons

1. A program completing without errors does not mean that its output is
   correct. The first implementation reported 43 detections even
   though the boxes were effectively invisible.

2. Inspecting intermediate numerical values was critical to debugging.
   Printing the raw `cx`, `cy`, `w` and `h` values immediately showed
   that the coordinates were already normalized.

3. Understanding the coordinate system is just as important as getting
   the neural-network inference to run. A correct model output can
   still produce an incorrect visualization if the coordinate
   conversion is wrong.

4. When a dependency installation becomes unnecessarily large for the
   actual objective, using a pre-converted model can be a reasonable
   engineering decision. In this case it avoided pulling PyTorch and
   CUDA dependencies into a VM that has no GPU.

### Final status

- YOLOv8n ONNX model downloaded and loaded successfully.
- ONNX input/output shapes verified.
- Object detection inference working.
- Bounding-box parsing working.
- Coordinate-conversion bug identified and fixed.
- Bounding boxes now visible on the output image.
- Multiple overlapping detections remain because NMS has not yet been
  implemented.

## Day 17 — Live Capture-to-Inference Loop 

Connected the AI pipeline directly to my driver for the first time - 
confirmed OpenCV can open /dev/video0 through the standard V4L2 backend 
(cv2.CAP_V4L2, cap.isOpened() = True) and read real frames (480x640x3 
confirmed) - an independent library talking to my driver exactly the 
way v4l2-ctl always has.

Built a synchronous loop over 100 frames: capture -> resize/preprocess 
-> YOLOv8n inference -> NMS -> draw boxes -> write video file.

**Result:** average capture 13.2ms, average inference 456.3ms, ~2.1fps 
effective. 0 detections across all frames - expected, since my driver's 
synthetic frames contain no real objects.

**Key finding:** capture (Days 4-13) is fast; inference is the clear 
bottleneck, over 30x slower than capture - the concrete, measured 
motivation for a producer/consumer design rather than a theoretical one.


## Day 18 — Proper Performance Measurement

Replaced basic timing with a real benchmarking tool: separated 
wall-clock time (time.time()) from CPU time (time.process_time()) for 
four pipeline stages (capture, preprocess, inference, overlay/NMS), 
logged every frame to CSV.

**Results (100 frames):**
- Capture: 5.6ms wall / 6.2ms CPU
- Preprocess: 10.1ms wall / 11.2ms CPU
- Inference: 153.7ms wall / 180.3ms CPU
- Overlay/NMS: 58.7ms wall / 193.6ms CPU
- Total: 228.1ms/frame, ~4.4fps

**Key finding:** confirmed inference as the dominant cost, and surfaced 
overlay/NMS as a real second-place cost simple timing hadn't isolated. 
Overlay's CPU time (193.6ms) exceeded its wall time (58.7ms) - a real 
signal that multiple threads were doing work in parallel during that 
stage, not a measurement error.

## Day 19 — Optimization Attempt: ONNX Runtime Thread Tuning

Targeted inference (the confirmed bottleneck) by explicitly configuring 
ONNX Runtime's intra_op_num_threads, matching my VM's actual core count 
(nproc = 2).

**Honest result: this made things worse on every configuration tried:**
- Baseline (default, no explicit config): 153.7ms wall
- intra_op_num_threads=2: 498.2-704.0ms wall (3.2-4.6x slower)
- intra_op_num_threads=1: 414.6-520.6ms wall (2.7-3.4x slower)

**Why:** on a 2-core VM, forcing ONNX Runtime to spawn dedicated worker 
threads creates contention with my main process, which is 
simultaneously doing capture and preprocessing on the same 2 physical 
cores. Context-switching overhead cost more than any parallelism 
gained - added parallelism only helps when there are genuinely idle 
cores to use, and my VM has none to spare.

**Decision:** kept ONNX Runtime's untouched default threading, since 
every manual configuration underperformed it. A legitimate negative 
result - correctly diagnosing why an "obvious" fix doesn't apply to 
hardware this constrained, rather than reporting a speedup that didn't 
happen.

**Does this block future work? No** - the core pipeline remains fully 
functional at its Day 18 baseline performance regardless of this result.


## Day 20 — README Documentation + Long-Running Stability Test 

Documented real Day 18 benchmark numbers in README.md, including the 
honest Day 19 finding.

Ran a genuine long-duration stability test: 2500 frames (~10 minutes 
continuous runtime) with memory tracked every 100 frames.

**Results:**
- Memory: 135.0MB at start, jumped to 169.0MB by frame 100 (one-time 
  warm-up allocation), then grew extremely slowly and linearly 
  afterward (169.0MB -> 171.2MB across the remaining 2400 frames) - 
  consistent with normal bookkeeping overhead, not a real memory leak.
- Performance: 221.1ms/frame average (4.5fps), matching Day 18's 
  228.1ms baseline closely - confirms measured performance is stable 
  and reproducible across a 25x longer run.

**Bug hit and fixed:** my first attempt at adding memory-tracking code 
placed an if-statement inside a dictionary literal 
(results.append({if i % 100 == 0: ...})) - invalid Python syntax, since 
a dictionary can only contain key-value pairs, not control-flow 
statements. Fixed by moving the check to its own statement after the 
dictionary was fully built and appended.

**Conclusion:** the pipeline is confirmed stable for extended use, not 
just short test runs.


## Day 21 (Week 3 Summary) — AI Inference Pipeline Complete 

**What I built, end to end:** ONNX Runtime setup and a classification 
sanity test, real object detection (YOLOv8n) with a genuine coordinate 
bug found and fixed, NMS to clean up duplicate boxes, live integration 
with my own V4L2 driver via OpenCV's standard V4L2 backend, proper 
wall-clock vs CPU benchmarking across pipeline stages, a documented 
failed optimization attempt, and a 10-minute stability test confirming 
no memory leak.

**What worked exactly as planned:** the full pipeline genuinely works 
end to end - real capture from my own kernel driver, real YOLOv8n 
inference, real NMS-cleaned detections, real reproducible performance 
numbers (221-228ms/frame, ~4.4-4.5fps across both a 100-frame and 
2500-frame test).

**What didn't work as hoped:** Day 19's thread-tuning optimization made 
inference slower in every configuration tried, due to thread contention 
on a 2-core VM with no spare cores for added parallelism to use. A 
legitimate negative result, not a hidden failure.

**Is "AI-ready" still honest?** Yes - the pipeline demonstrably reads 
real driver frames and runs real object detection with measured, 
reproducible results. What I don't claim is 30fps real-time performance 
- the honest number is ~4.4fps on this constrained VM, stated plainly 
in the README rather than oversold.

**Next in Week 4:** attempt the DMA-BUF zero-copy stretch goal, treated 
as genuinely optional per the roadmap's own decision-point guidance.


## Day 22-23 — DMA-BUF Stretch Goal: Attempted, Confirmed Blocked

Studied DMA-BUF's attach/map/fence model and why it needs physically 
contiguous memory (for real hardware like a GPU to read directly, 
unlike vmalloc's virtually-contiguous-but-possibly-scattered pages). 
Correctly predicted in my notes that switching to 
vb2_dma_contig_memops would need a real struct device I've never had.

Switched vb2_q.mem_ops to vb2_dma_contig_memops. Module loaded and 
registered cleanly - but that alone didn't prove anything, since 
vb2_queue_init only configures the queue, it doesn't allocate real 
buffer memory yet.

The actual test - requesting buffers via v4l2-ctl --stream-mmap - 
confirmed the predicted blocker: "VIDIOC_REQBUFS returned -1 (Cannot 
allocate memory)", with a kernel WARNING trace. Root cause: 
vb2_dma_contig_memops requires a valid struct device (vb2_q.dev) for 
DMA addressing, which this driver has never had, since 
v4l2_device_register was deliberately called with NULL as the parent 
(no real hardware exists for this synthetic camera).

**Decision, per the roadmap's own guidance:** stopped here rather than 
fabricating a fake struct device just to satisfy this requirement, 
which would add complexity with no real hardware to actually 
demonstrate zero-copy against. Redirecting remaining time to benchmark 
comparison, cleanup, and documentation - the guaranteed core deliverable 
(Days 4-21) remains fully complete regardless of this outcome.

## Day 24 — Benchmark Comparison + Session-Length Performance Pattern

Re-ran Day 18's benchmark to investigate an anomalous result (1452.8ms/
frame) seen once after Day 20's long stability test. Got a second slow 
result (714.7ms) in a similarly long session, confirming it wasn't a 
one-off fluke.

**Pattern identified:** performance is consistently fast (~220-230ms/
frame) early in a fresh VM session, and consistently degrades 
(500-1450ms/frame, 2-6x slower) later in longer sessions - reproducible 
across multiple separate sessions, regardless of ONNX threading 
configuration. Points to VM/host-level resource accumulation over 
session length, not anything specific to my driver or inference code.

Built a benchmark comparison table (docs/benchmark-comparison.md) 
documenting all runs side by side with session context, rather than 
presenting one cherry-picked number.


## Day 25 — Decision Checkpoint: DMA-BUF Officially Stopped, Cleanup Begins

Formal checkpoint confirming the Day 22-23 blocker is a genuine 
architectural boundary, not something to keep pushing on. Began driver 
code cleanup - scanned for leftover debug code, added a proper header 
comment to myv4l2.c documenting what the driver is and its development 
lineage.


## Day 26 — README Structure Research + Benchmark Table Finalization

Looked at a few other embedded/driver project READMEs on GitHub for 
structural inspiration - confirmed my planned structure matches how 
real technical projects present themselves. Merged the full Day 24 
benchmark comparison table into the main README rather than a 
single-run number.


## Day 27 — Full README Rewrite 

Rewrote README.md completely to reflect actual current status - full 
status table, real benchmark numbers with the session-variability 
caveat, and an honest Limitations section covering CPU-only inference, 
the reverted thread-tuning attempt, the DMA-BUF architectural blocker, 
and the synthetic camera scope decision.


## Day 28 — Code Cleanup + checkpatch.pl 

Ran checkpatch.pl against myv4l2.c - returned a high count of style 
warnings (mostly spacing/brace conventions and trailing whitespace), 
consistent with a first kernel driver not written to strict kernel 
style from the start, not a functional issue.

Fixed the easy, safe-to-automate categories: trailing whitespace 
(sed -i 's/[ \t]*$//'), missing space after if/for/while before 
parentheses. Rebuilt and confirmed identical behavior after cleanup.

Chose not to chase remaining style warnings to zero, given limited 
remaining time and zero functional impact.

## Day 29 — Demo Recording 

Recorded a demonstration covering both halves of the project: the 
driver loading, streaming, and benchmark script showing real per-stage 
timing and FPS from /dev/video0, and the detection pipeline running 
against a real test photo, showing actual bounding boxes and a printed 
detection count.
