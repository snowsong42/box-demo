from pathlib import Path
from PIL import Image

BASE = Path(__file__).parent
OUTPUT = BASE / "output"
OUTPUT.mkdir(exist_ok=True)

# 支持的图片后缀
EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tiff", ".tif"}

# 收集图片并排序（按文件名保证顺序稳定）
images = sorted(
    [f for f in BASE.iterdir() if f.suffix.lower() in EXTS],
    key=lambda f: f.name
)

for idx, img_path in enumerate(images, start=1):
    out_name = f"{idx:04d}.png"
    out_path = OUTPUT / out_name

    with Image.open(img_path) as img:
        img = img.convert("RGBA")

        # 中心裁剪为正方形
        w, h = img.size
        side = min(w, h)
        left = (w - side) // 2
        top = (h - side) // 2
        img = img.crop((left, top, left + side, top + side))

        # 缩放为 200x200
        img = img.resize((200, 200), Image.LANCZOS)

        img.save(out_path, "PNG")

    print(f"[{idx:04d}] {img_path.name} → {out_name}")

print(f"\n完成！共处理 {len(images)} 张图片，输出至 {OUTPUT}")
