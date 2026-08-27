# V4L2 Driver Flow — My Notes (Day 3)

## Module Load Sequence (what happens when the driver loads)
1. module_init runs (same entry-point mechanism as my Day 1 hello_init)
2. A v4l2_device gets registered via v4l2_device_register() - this 
   tells the kernel's V4L2 core "a new video subsystem instance exists"
3. A video_device struct gets allocated and filled in - name, which 
   v4l2_device it belongs to, what operations it supports
4. That video_device gets registered via video_register_device() - 
   THIS is the step that actually creates /dev/videoX, automatically. 
   This is the automatic version of what I did manually with `mknod` 
   on Day 2.
5. From here, an app calling open("/dev/video0") gets routed into my 
   driver's functions - same redirection mechanism as Day 2's 
   file_operations, just wrapped in V4L2's own API layer.

## Key Structures
- v4l2_device: represents the overall camera subsystem/controller - 
  the "parent" that can own one or more actual video devices. Doesn't 
  handle file operations directly - more of a coordinator/registry.
- video_device: represents ONE actual /dev/videoX node - this is the 
  thing an application actually opens. Directly comparable to my Day 2 
  character device, but V4L2-specific.

## Why two separate structures instead of one?
Because real hardware can have one physical camera board exposing 
multiple video streams (e.g. color + depth sensor on one board). 
v4l2_device is the shared "parent" for all of them; each individual 
stream gets its own video_device. For my project (one synthetic 
camera), I'll have exactly one of each - but the split exists for 
real multi-sensor hardware.

## How an app eventually gets to /dev/videoX
Module loads -> v4l2_device registers (subsystem exists) -> 
video_device gets configured and registered (THIS creates /dev/videoX) 
-> app calls open() on that path -> kernel routes the call into my 
driver's functions, same mechanism as Day 2's file_operations table, 
just through V4L2's wrapper.

## Diagram
![V4L2 flow diagram]("C:\Users\Unnati\Downloads\flowchart.jpeg")
