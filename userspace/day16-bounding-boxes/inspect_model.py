import onnxruntime as ort

session = ort.InferenceSession("yolov8n.onnx")
print("Input name:", session.get_inputs()[0].name)
print("Input shape:", session.get_inputs()[0].shape)
print("Output name:", session.get_outputs()[0].name)
print("Output shape:", session.get_outputs()[0].shape)

