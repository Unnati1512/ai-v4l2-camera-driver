# AI-Ready V4L2 Camera Driver with Real-Time Frame Processing for Embedded Linux

A custom Linux kernel driver built on the V4L2 (Video4Linux2) standard, 
paired with a real-time AI inference application that is built from scratch 
to understand the full pipeline from kernel-space device drivers to 
real-time computer vision.

## What This Project Does

This project implements:
- A custom V4L2-compliant character device driver for Linux
- Kernel space buffer management for video frame capture (videobuf2)
- Pixel format negotiation and buffer request/queue/dequeue handling
- A synthetic frame source, standing in for real camera hardware, so 
  the focus stays on the driver architecture itself
- A userspace application that reads frames from the driver and runs 
  real-time AI object detection on them (coming in Week 3)

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

 **In progress — Day 10 of 30**

| Component | Status |
|---|---|
| Kernel module basics |  Complete |
| Character device driver |  Complete |
| V4L2 device registration (v4l2_device, video_device) |  Complete |
| VIDIOC_QUERYCAP |  Complete |
| vb2 buffer queue setup |  Complete |
| Pixel format negotiation (ENUM_FMT/G_FMT/S_FMT) |  Complete |
| REQBUFS/QUERYBUF/QBUF/DQBUF |  Complete |
| STREAMON/STREAMOFF + synthetic frame generator |  In progress |
| AI inference integration |  Planned |
| DMA-BUF zero-copy (stretch goal) |  Planned |

## Project Structure

driver/ — kernel module source code, organized by development day
docs/ - daily development log, architecture notes, diagrams
userspace/ - AI inference consumer application (coming in Week 3)


## Development Log

Full day by day build log, including bugs hit, root causes, and what I 
learned from each, is in [docs/devlog.md](docs/devlog.md).

## Tech Stack

- Linux kernel module development (C)
- V4L2 (Video4Linux2) subsystem, videobuf2 (vb2) buffer framework
- ONNX Runtime / real-time object detection (planned, Week 3)

## Author

Unnati Chaturvedi - Final year ECE student at Pandit Deendayal Energy University, building toward embedded AI/hardware 
engineering roles.
