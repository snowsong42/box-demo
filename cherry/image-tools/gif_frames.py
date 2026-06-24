from pathlib import Path
from PIL import Image

BASE = Path(__file__).parent
OUTPUT = BASE / "output"
OUTPUT.mkdir(exist_ok=True)

gif = Image.open(BASE / "cat.GIF")

for i in range(gif.n_frames):
    gif.seek(i)
    frame = gif.copy().convert("RGBA")

    # 中心裁剪为正方形
    w, h = frame.size
    side = min(w, h)
    left = (w - side) // 2
    top = (h - side) // 2
    frame = frame.crop((left, top, left + side, top + side))

    # 缩放为 200x200
    frame = frame.resize((200, 200), Image.LANCZOS)

    out_name = f"gif_{i + 1:04d}.png"
    frame.save(OUTPUT / out_name, "PNG")
    print(f"[{i + 1:04d}] → {out_name}")

print(f"\n完成！共提取 {gif.n_frames} 帧，输出至 {OUTPUT}")
