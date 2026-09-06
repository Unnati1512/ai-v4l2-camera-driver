import sys
import onnxruntime as ort
import numpy as np
import cv2
from PIL import Image, ImageDraw

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

if len(sys.argv) != 2:
    print(f"Usage: python3 {sys.argv[0]} <image>")
    sys.exit(1)

image_path = sys.argv[1]

session = ort.InferenceSession("../yolov8n.onnx")
input_name = session.get_inputs()[0].name

img = Image.open(image_path).convert("RGB")
orig_w, orig_h = img.size

print(f"Input image: {image_path}")
print(f"Original image size: {orig_w} x {orig_h}")

img_resized = img.resize((INPUT_SIZE, INPUT_SIZE))

arr = np.array(img_resized).astype(np.float32) / 255.0
arr = arr.transpose(2, 0, 1)
arr = np.expand_dims(arr, axis=0)

outputs = session.run(None, {input_name: arr})

predictions = outputs[0][0].T

print(f"Raw predictions: {predictions.shape}")

boxes = []
scores = []
class_ids = []

for pred in predictions:

    cx, cy, w, h = pred[:4]

    class_scores = pred[4:]

    class_id = int(np.argmax(class_scores))
    confidence = float(class_scores[class_id])

    if confidence < CONF_THRESHOLD:
        continue

    x1 = (cx - w / 2) * orig_w
    y1 = (cy - h / 2) * orig_h
    x2 = (cx + w / 2) * orig_w
    y2 = (cy + h / 2) * orig_h

    x1 = max(0, int(x1))
    y1 = max(0, int(y1))
    x2 = min(orig_w - 1, int(x2))
    y2 = min(orig_h - 1, int(y2))

    box_w = x2 - x1
    box_h = y2 - y1

    if box_w <= 0 or box_h <= 0:
        continue

    boxes.append([x1, y1, box_w, box_h])
    scores.append(confidence)
    class_ids.append(class_id)

print(f"Candidates before NMS: {len(boxes)}")

# Perform NMS separately for each class
kept_indices = []

for current_class in sorted(set(class_ids)):

    class_indices = [
        i for i, cid in enumerate(class_ids)
        if cid == current_class
    ]

    class_boxes = [boxes[i] for i in class_indices]
    class_scores = [scores[i] for i in class_indices]

    indices = cv2.dnn.NMSBoxes(
        class_boxes,
        class_scores,
        CONF_THRESHOLD,
        NMS_THRESHOLD
    )

    if len(indices) > 0:
        indices = np.array(indices).flatten()

        for idx in indices:
            kept_indices.append(class_indices[idx])

print(f"Detections after NMS: {len(kept_indices)}")

draw = ImageDraw.Draw(img)

for i in kept_indices:

    x, y, w, h = boxes[i]

    class_id = class_ids[i]
    confidence = scores[i]

    x2 = x + w
    y2 = y + h

    draw.rectangle(
        [x, y, x2, y2],
        outline="red",
        width=3
    )

    label = f"{COCO_CLASSES[class_id]} {confidence:.2f}"

    draw.text(
        (x, max(y - 15, 0)),
        label,
        fill="red"
    )

    print(
        f"Detection: {COCO_CLASSES[class_id]} "
        f"confidence={confidence:.2f} "
        f"box=({x},{y})-({x2},{y2})"
    )

output_path = "detected_nms_output.png"
img.save(output_path)

print(f"Saved {output_path}")
