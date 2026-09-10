# Benchmark Results Summary

| Run | Frames | Total avg (ms) | FPS | Session context |
|---|---|---|---|---|
| Day 18 baseline | 100 | 228.1 | 4.4 | Early in session, fresh module load |
| Day 19 (2 threads forced) | 100 | 704.0 | 1.4 | Same session as Day 18, later |
| Day 19 (1 thread forced) | 100 | 520.6 | 1.9 | Same session, later still |
| Day 20 (2500-frame stability) | 2500 | 221.1 | 4.5 | Fresh session, early |
| Day 24 re-check #1 | 100 | 1452.8 | 0.7 | Later in a long session (after 2500-frame test) |
| Day 24 re-check #2 | 100 | 714.7 | 1.4 | Later in a long session |


**Pattern observed:** performance is consistently fast (~220-230ms/frame) 
early in a fresh VM session, and consistently degrades (500-1450ms/frame) 
later in longer sessions, regardless of which specific ONNX threading 
configuration is used. This suggests the degradation is a VM/host-level 
resource accumulation issue (background processes, VirtualBox overhead 
building up over session length), not something specific to the driver 
or inference code itself - the SAME code produces both fast and slow 
results depending on session freshness.
