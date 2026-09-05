import onnxruntime as ort
import numpy as np
from PIL import Image

session = ort.InferenceSession("mobilenetv2.onnx")
input_name = session.get_inputs()[0].name

img = Image.open("checkerboard_test.png").convert("RGB")
img = img.resize((224, 224))
arr = np.array(img).astype(np.float32) / 255.0
arr = arr.transpose(2, 0, 1)
arr = np.expand_dims(arr, axis=0)

outputs = session.run(None, {input_name: arr})
predictions = outputs[0][0]
top_class = np.argmax(predictions)
print(f"Top predicted class index: {top_class}")
print(f"Confidence score: {predictions[top_class]:.4f}")
