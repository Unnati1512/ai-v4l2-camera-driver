import onnxruntime as ort
import numpy as np
from PIL import Image, ImageDraw

COCO_CLASSES = ["person", "bicycle", "car", "motorcycle", "airplane", "bus",
                "train", "truck", "boat"]

CONF_THRESHOLD = 0.5
INPUT_SIZE = 640

session = ort.InferenceSession("yolov8n.onnx")
input_name = session.get_inputs()[0].name

img = Image.open("test_photo.jpg").convert("RGB")
orig_w, orig_h = img.size

img_resized = img.resize((INPUT_SIZE, INPUT_SIZE))
arr = np.array(img_resized).astype(np.float32) / 255.0
arr = arr.transpose(2, 0, 1)
arr = np.expand_dims(arr, axis=0)

outputs = session.run(None, {input_name: arr})
predictions = outputs[0][0]
predictions = predictions.T

draw = ImageDraw.Draw(img)
detections_found = 0

for pred in predictions:
    box = pred[:4]
    class_scores = pred[4:]
    class_id = np.argmax(class_scores)
    confidence = class_scores[class_id]

    if confidence < CONF_THRESHOLD:
        continue
    if class_id >= len(COCO_CLASSES):
        continue

    cx, cy, w, h = box
    # cx, cy, w, h are already normalized (0-1) fractions of the image -
    # multiply directly by original width/height, no division by INPUT_SIZE
    x1 = (cx - w / 2) * orig_w
    y1 = (cy - h / 2) * orig_h
    x2 = (cx + w / 2) * orig_w
    y2 = (cy + h / 2) * orig_h

    draw.rectangle([x1, y1, x2, y2], outline="red", width=3)
    label = f"{COCO_CLASSES[class_id]} {confidence:.2f}"
    draw.text((x1, max(y1 - 12, 0)), label, fill="red")
    detections_found += 1

print(f"Detections drawn: {detections_found}")
img.save("detected_output.png")
print("Saved detected_output.png")
