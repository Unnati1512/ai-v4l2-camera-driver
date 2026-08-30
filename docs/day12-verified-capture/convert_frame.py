import numpy as np
from PIL import Image

WIDTH = 640
HEIGHT = 480
FRAME_SIZE = WIDTH * HEIGHT * 2  # YUYV = 2 bytes per pixel

FRAME_INDEX = 4
with open("captured_frames.raw", "rb") as f:
    f.seek(FRAME_INDEX * FRAME_SIZE)
    data = f.read(FRAME_SIZE)  # just the first frame

yuyv = np.frombuffer(data, dtype=np.uint8).reshape((HEIGHT, WIDTH, 2))

# Extract Y (luminance) channel only for a simple grayscale preview
y = yuyv[:, :, 0]

img = Image.fromarray(y, mode='L')
img.save("frame4_preview.png")
print("Saved frame4_preview.png")
