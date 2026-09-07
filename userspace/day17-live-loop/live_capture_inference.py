import cv2
import numpy as np
import onnxruntime as ort
import time

COCO_CLASSES = [
    "person", "bicycle", "car", "motorcycle", "airplane",
    "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench",
    "bird", "cat", "dog", "horse", "sheep",
    "cow", "elephant", "bear", "zebra", "giraffe",
    "backpack", "umbrella", "handbag", "tie", "suitcase",
    "frisbee", "skis", "snowboard", "sports ball",
    "kite", "baseball bat", "baseball glove", "skateboard",
    "surfboard", "tennis racket", "bottle", "wine glass",
    "cup", "fork", "knife", "spoon", "bowl",
    "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza",
    "donut", "cake", "chair", "couch", "potted plant",
    "bed", "dining table", "toilet", "tv", "laptop",
    "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink",
    "refrigerator", "book", "clock", "vase",
    "scissors", "teddy bear", "hair drier", "toothbrush"
]

CONF_THRESHOLD = 0.5
NMS_THRESHOLD = 0.45
INPUT_SIZE = 640
NUM_FRAMES_TO_CAPTURE = 100

session = ort.InferenceSession("yolov8n.onnx")
input_name = session.get_inputs()[0].name

cap = cv2.VideoCapture('/dev/video0', cv2.CAP_V4L2)
if not cap.isOpened():
    print("ERROR: could not open /dev/video0")
    exit(1)

frame_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
frame_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
print(f"Capture opened: {frame_w}x{frame_h}")

fourcc = cv2.VideoWriter_fourcc(*'mp4v')
out = cv2.VideoWriter('live_output.mp4', fourcc, 10, (frame_w, frame_h))

frame_times = []
total_detections = 0

for i in range(NUM_FRAMES_TO_CAPTURE):
    t_capture_start = time.time()
    ret, frame = cap.read()
    t_capture_end = time.time()

    if not ret:
        print(f"Frame {i}: capture failed, skipping")
        continue

    t_infer_start = time.time()

    img_resized = cv2.resize(frame, (INPUT_SIZE, INPUT_SIZE))
    arr = img_resized.astype(np.float32) / 255.0
    arr = arr.transpose(2, 0, 1)
    arr = np.expand_dims(arr, axis=0)

    outputs = session.run(None, {input_name: arr})
    predictions = outputs[0][0].T

    t_infer_end = time.time()

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
    if len(keep) > 0:
        keep = np.array(keep).flatten()

    for idx in keep:
        x, y, bw, bh = boxes[idx]
        label = f"{COCO_CLASSES[class_ids[idx]]} {scores[idx]:.2f}"
        cv2.rectangle(frame, (x, y), (x + bw, y + bh), (0, 0, 255), 2)
        cv2.putText(frame, label, (x, max(y - 5, 0)), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)

    total_detections += len(keep)

    capture_ms = (t_capture_end - t_capture_start) * 1000
    infer_ms = (t_infer_end - t_infer_start) * 1000
    frame_times.append((capture_ms, infer_ms))

    out.write(frame)

    if i % 10 == 0:
        print(f"Frame {i}: capture={capture_ms:.1f}ms infer={infer_ms:.1f}ms detections={len(keep)}")

cap.release()
out.release()

avg_capture = sum(t[0] for t in frame_times) / len(frame_times)
avg_infer = sum(t[1] for t in frame_times) / len(frame_times)
print(f"\nProcessed {len(frame_times)} frames")
print(f"Average capture time: {avg_capture:.1f}ms")
print(f"Average inference time: {avg_infer:.1f}ms")
print(f"Total detections across all frames: {total_detections}")
print(f"Effective FPS (capture+infer): {1000/(avg_capture+avg_infer):.1f}")
print("Saved live_output.mp4")
