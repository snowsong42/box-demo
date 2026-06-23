"""Convert 500x150.jpg to RGB565 raw binary for ESP32 pushImage."""
from PIL import Image
img = Image.open("../resource/500x150.jpg").convert("RGB")
w, h = img.size
pixels = list(img.getdata())
raw = bytearray(w * h * 2)
for i, (r, g, b) in enumerate(pixels):
    v = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)  # BGR565
    raw[i * 2] = v & 0xFF
    raw[i * 2 + 1] = (v >> 8) & 0xFF
with open("../resource/500x150.raw", "wb") as f:
    f.write(raw)
print(f"OK: {w}x{h} -> 500x150.raw ({len(raw)} bytes)")
