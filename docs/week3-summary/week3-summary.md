# Week 3 Summary — AI Inference Pipeline

## What I built, end to end

Starting from Day 15's ONNX Runtime setup, I built a complete AI 
inference pipeline: a classification model test (MobileNetV2) to prove 
the basic load-and-infer mechanism worked, then a real object detection 
model (YOLOv8n) with bounding box visualization, which required fixing 
a genuine coordinate-normalization bug before boxes rendered correctly. 
I added Non-Max Suppression to clean up duplicate overlapping 
detections, then connected the entire pipeline to my own V4L2 driver - 
OpenCV opens /dev/video0 through the standard V4L2 backend and reads 
real frames, the same way any standard V4L2 application would. I built 
a proper benchmarking tool separating wall-clock from CPU time across 
four pipeline stages, attempted a thread-tuning optimization, and 
finished with a 10-minute, 2500-frame stability test confirming no 
memory leak and reproducible performance.

## What worked exactly as planned

The full pipeline genuinely works end to end: real capture from my own 
kernel driver, real YOLOv8n inference, real NMS-cleaned detections, and 
real, independently-verified performance numbers (221-228ms/frame, 
~4.4-4.5fps, consistent across both a 100-frame and a 2500-frame test). 
Every stage is measurable and logged to CSV, not just asserted.

## What didn't work as hoped, and why that's still a valid result

Day 19's optimization attempt - explicitly configuring ONNX Runtime's 
thread count to match my VM's core count - made inference slower, not 
faster, in both configurations I tried (2 threads: 3.2x slower; 1 
thread: 2.7x slower than the untouched default). The reason is thread 
contention: my 2-core VM has no spare cores for ONNX Runtime's worker 
threads to use without competing with my main capture/preprocessing 
process for the same physical cores, so the added parallelism cost more 
in context-switching overhead than it gained. I kept the default 
threading configuration since it outperformed every manual setting I 
tried. This is a legitimate negative result, not a failure to hide - it 
demonstrates I correctly diagnosed why an "obvious" fix doesn't apply 
to hardware this constrained, rather than blindly reporting an 
improvement that didn't actually happen.

## Is the "AI-ready" claim in my project title still honest?

Yes. The pipeline demonstrably reads real frames from my own driver and 
runs a real, standard object detection model on them, with measured, 
reproducible results - that's what "AI-ready" claims, and it's true. 
What I'm careful not to claim is 30fps real-time performance - on this 
2-core VM with CPU-only inference, the honest number is ~4.4fps. 

