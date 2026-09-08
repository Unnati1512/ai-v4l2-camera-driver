import cv2
import numpy as np
import onnxruntime as ort
import time
import csv
import resource

CONF_THRESHOLD = 0.5
NMS_THRESHOLD = 0.45
INPUT_SIZE = 640
NUM_FRAMES = 2500

session = ort.InferenceSession("yolov8n.onnx")
input_name = session.get_inputs()[0].name

cap = cv2.VideoCapture('/dev/video0', cv2.CAP_V4L2)
if not cap.isOpened():
    print("ERROR: could not open /dev/video0")
    exit(1)

frame_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
frame_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

results = []

for i in range(NUM_FRAMES):
    wall_capture_start = time.time()
    cpu_capture_start = time.process_time()
    ret, frame = cap.read()
    wall_capture_end = time.time()
    cpu_capture_end = time.process_time()

    if not ret:
        continue

    wall_preprocess_start = time.time()
    cpu_preprocess_start = time.process_time()
    img_resized = cv2.resize(frame, (INPUT_SIZE, INPUT_SIZE))
    arr = img_resized.astype(np.float32) / 255.0
    arr = arr.transpose(2, 0, 1)
    arr = np.expand_dims(arr, axis=0)
    wall_preprocess_end = time.time()
    cpu_preprocess_end = time.process_time()

    wall_infer_start = time.time()
    cpu_infer_start = time.process_time()
    outputs = session.run(None, {input_name: arr})
    wall_infer_end = time.time()
    cpu_infer_end = time.process_time()

    wall_overlay_start = time.time()
    cpu_overlay_start = time.process_time()
    predictions = outputs[0][0].T
    boxes, scores, class_ids = [], [], []
    for pred in predictions:
        cx, cy, w, h = pred[:4]
        class_scores = pred[4:]
        class_id = int(np.argmax(class_scores))
        confidence = float(class_scores[class_id])
        if confidence < CONF_THRESHOLD:
            continue
        x1 = int((cx - w / 2) * frame_w)
        y1 = int((cy - h / 2) * frame_h)
        x2 = int((cx + w / 2) * frame_w)
        y2 = int((cy + h / 2) * frame_h)
        bw, bh = x2 - x1, y2 - y1
        if bw <= 0 or bh <= 0:
            continue
        boxes.append([x1, y1, bw, bh])
        scores.append(confidence)
        class_ids.append(class_id)
    keep = cv2.dnn.NMSBoxes(boxes, scores, CONF_THRESHOLD, NMS_THRESHOLD) if boxes else []
    wall_overlay_end = time.time()
    cpu_overlay_end = time.process_time()

    results.append({
        "frame": i,
        "capture_wall_ms": (wall_capture_end - wall_capture_start) * 1000,
        "capture_cpu_ms": (cpu_capture_end - cpu_capture_start) * 1000,
        "preprocess_wall_ms": (wall_preprocess_end - wall_preprocess_start) * 1000,
        "preprocess_cpu_ms": (cpu_preprocess_end - cpu_preprocess_start) * 1000,
        "infer_wall_ms": (wall_infer_end - wall_infer_start) * 1000,
        "infer_cpu_ms": (cpu_infer_end - cpu_infer_start) * 1000,
        "overlay_wall_ms": (wall_overlay_end - wall_overlay_start) * 1000,
        "overlay_cpu_ms": (cpu_overlay_end - cpu_overlay_start) * 1000,
        "detections": len(keep) if len(keep) > 0 else 0,
    })
    if i % 100 == 0:
        mem_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        print(f"Frame {i}: memory usage = {mem_kb / 1024:.1f} MB")

cap.release()

with open("benchmark_results.csv", "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=results[0].keys())
    writer.writeheader()
    writer.writerows(results)

def avg(key):
    return sum(r[key] for r in results) / len(results)

print(f"Processed {len(results)} frames\n")
print(f"{'Stage':<15} {'Wall (ms)':<12} {'CPU (ms)':<12}")
print(f"{'Capture':<15} {avg('capture_wall_ms'):<12.1f} {avg('capture_cpu_ms'):<12.1f}")
print(f"{'Preprocess':<15} {avg('preprocess_wall_ms'):<12.1f} {avg('preprocess_cpu_ms'):<12.1f}")
print(f"{'Inference':<15} {avg('infer_wall_ms'):<12.1f} {avg('infer_cpu_ms'):<12.1f}")
print(f"{'Overlay/NMS':<15} {avg('overlay_wall_ms'):<12.1f} {avg('overlay_cpu_ms'):<12.1f}")
total_wall = avg('capture_wall_ms') + avg('preprocess_wall_ms') + avg('infer_wall_ms') + avg('overlay_wall_ms')
print(f"\nTotal avg wall time per frame: {total_wall:.1f}ms")
print(f"Effective FPS: {1000/total_wall:.1f}")
print("Saved benchmark_results.csv")
