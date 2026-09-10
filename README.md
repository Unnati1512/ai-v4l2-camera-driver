# AI-Ready V4L2 Camera Driver with Real-Time Frame Processing for Embedded Linux

A custom Linux kernel driver built on the V4L2 (Video4Linux2) standard, 
paired with a real-time AI inference application that is built from scratch 
to understand the full pipeline from kernel-space device drivers to 
real-time computer vision.

## What This Project Does

This project implements:
- A custom V4L2-compliant kernel driver for Linux, including device 
  registration, buffer queue management (videobuf2), pixel format 
  negotiation, and the full streaming lifecycle
- A synthetic frame source standing in for real camera hardware, so 
  the focus stays on the driver architecture and buffer-management 
  mechanics rather than sensor-specific hardware programming
- A userspace AI inference application (Python, OpenCV, ONNX Runtime) 
  that reads real frames directly from the driver via the standard 
  V4L2 interface and runs YOLOv8n object detection on them, with 
  Non-Max Suppression and bounding box visualization
- Full performance benchmarking (wall-clock and CPU time, per pipeline 
  stage) and a 10-minute stability test confirming no memory leaks

## Why I Built This

I use cameras, video calls, and AI powered apps every day and never 
actually understood what's happening underneath like how does an 
operating system "see" a camera at all? What does the kernel actually 
do when an app asks for a video frame? 

This project is my attempt to go below the application layer for 
once - not use a library, but actually build and understand the 
mechanism. It's also strategically the right kind of depth for the 
embedded/AI hardware roles I'm targeting, but that's not the only 
reason, and I don't want it to be it's the curiosity is what's actually 
carried me through the harder debugging sessions. This project is my 
attempt to go one level deeper.

## Current Status

 **Core project complete** (Days 1-21) — V4L2 driver, buffer queue, 
streaming, and AI inference pipeline are all built, tested, and 
verified end to end. Currently in the documentation/polish phase.

| Component | Status |
|---|---|
| Kernel module basics |  Complete |
| Character device driver |  Complete |
| V4L2 device registration (v4l2_device, video_device) |  Complete |
| VIDIOC_QUERYCAP | Complete |
| vb2 buffer queue setup | Complete |
| Pixel format negotiation (ENUM_FMT/G_FMT/S_FMT) |  Complete |
| REQBUFS/QUERYBUF/QBUF/DQBUF | Complete |
| STREAMON/STREAMOFF + synthetic frame generator |  Complete |
| Robustness testing (concurrent access, safe removal) |  Complete |
| AI inference integration (YOLOv8n, NMS, live capture) |  Complete |
| Performance benchmarking & stability testing |  Complete |
| DMA-BUF zero-copy (stretch goal) | ⚠️Attempted, documented blocker (see Limitations) |


## Benchmarks

| Stage | Wall time (avg) |
|---|---|
| Capture | ~5-15ms |
| Preprocess | ~10-40ms |
| Inference (YOLOv8n, CPU) | ~150-450ms |
| Overlay/NMS | ~55-220ms |
| **Total** | **~220-230ms/frame under normal conditions (~4.4fps)** |

Measured on a 2-core VirtualBox VM, CPU-only inference. Performance 
was observed to vary meaningfully with session length/VM resource 
state (see Limitations) — the ~220-230ms figure reflects consistent, 
reproducible results on a freshly-loaded system.


## Limitations

- **CPU-only inference:** no GPU passthrough available in this 
  VirtualBox environment. Inference is the dominant per-frame cost.
- **Thread tuning attempted, reverted:** explicitly configuring ONNX 
  Runtime's thread count (to match the VM's 2 physical cores) made 
  performance significantly worse (2-3x slower) due to thread 
  contention, not better. Kept the runtime's untouched default 
  threading, which consistently outperformed manual configuration.
- **DMA-BUF zero-copy not achieved:** attempted switching to 
  `vb2_dma_contig_memops` for real DMA-BUF export. Confirmed this 
  requires a valid `struct device` representing real hardware, which 
  this driver deliberately doesn't have (it's a software-defined 
  synthetic camera, registered with `NULL` as its parent device since 
  Day 4). This is a genuine architectural boundary of a hardware-free 
  driver, not an unresolved bug — documented in detail in 
  `docs/day22-dmabuf-study/`.
- **Performance variability observed:** benchmark results were 
  noticeably slower (up to 6x) later in long VM sessions compared to 
  a freshly-booted state, suggesting host/VM resource accumulation 
  over session length rather than an issue in the driver or inference 
  code itself.
- **Synthetic camera, not real hardware:** the driver's "camera" is a 
  kernel-thread-generated solid-color frame source, not a real image 
  sensor. This is an intentional scope decision (see Development Log) 
  that let the project focus on kernel buffer management and V4L2 
  architecture rather than sensor-specific programming.

## Project Structure

driver/ — kernel module source code, organized by development day
docs/ — daily development log, architecture notes, diagrams,
benchmark data
userspace/ — AI inference consumer application

## Development Log

Full day-by-day build log, including every bug hit, its root cause, 
and what I learned from each, is in [docs/devlog.md](docs/devlog.md).


## Tech Stack

- Linux kernel module development (C), V4L2 subsystem, videobuf2 (vb2) buffer framework
- Python, OpenCV, ONNX Runtime for the AI inference application
- YOLOv8n for real-time object detection

## Author

Unnati Chaturvedi - Final year ECE student at Pandit Deendayal Energy University, building toward embedded AI/hardware 
engineering roles.
