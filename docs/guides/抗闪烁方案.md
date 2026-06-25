# SPI 显示屏抗闪烁：从逐步修复到全帧缓冲

> 记录 box-demo 项目中消除 ST7789 SPI 显示屏"逐行刷新闪烁"问题的完整演进历程与最终方案。

---

## 1. 问题本质

SPI 显示屏（如 ST7789）的像素通过 SPI 总线逐行写入内部 GRAM。当写入速度慢于视觉感知时，会看到**画面从上到下逐步刷新**，表现为"雨刷"效果或闪烁。

```
根因链:
  先清屏 fillRect(320×240, BLACK)
  → 154KB 数据通过 SPI 逐行推入 (约 25ms)
  → 用户看到黑底从左上到右下逐步覆盖
  → 再绘制新内容
  → 新内容又逐行出现
  → 整体体验: 黑闪 → 逐行刷新
```

## 2. 演进历程

### Phase 1: 局部 `fillRect` 清除（失败）

**做法**：在绘制图片前，用 `fillRect` 清除图片显示区域。

```cpp
tft.fillRect(0, 20, 320, 200, COLOR_BLACK);  // 清图片区
tft.drawPng(buf, size, x, y);                  // 画新图
```

**现象**：黑底逐行刷下 → 图片逐行出现，产生"黑色雨刷刷下来"的效果。

**原因**：`fillRect` 和 `drawPng` 在同一个 `startWrite`/`endWrite` 内，但 `drawPng` 解码 PNG 需要 CPU 时间，解码期间屏幕已显示 `fillRect` 的黑底。等解码完毕再推像素时，用户已看到黑底。

**结论**：直接对显示屏做"先清后画"必然产生中间态。关键不是在哪个区域清、清多大，而是要消除中间态的可见性。

---

### Phase 2: 离屏 Sprite 合成（部分成功）

**做法**：在 PSRAM 中创建离屏 `LGFX_Sprite`，完成"黑底→画PNG→合成"，最后一次性 `pushSprite` 到屏幕。

```cpp
LGFX_Sprite spr(&tft);
spr.createSprite(320, 200);
spr.fillScreen(COLOR_BLACK);
spr.drawPng(buf, size, sx, sy);    // 合成在 PSRAM 中，屏幕不可见
spr.pushSprite(0, 20);             // 一次性 DMA 推屏
```

**效果**：消除了黑底→图片的中间态，但仍能看到 `pushSprite` 的 128KB 数据逐行推入屏幕。

**结论**：离屏合成解决了"先黑后图"的闪烁，但没有解决"逐行刷新"本身。

---

### Phase 3: ST7789 `0x28`/`0x29` 显存控制（部分成功）

**做法**：利用 ST7789 的 `Display Off (0x28)` 命令——屏幕输出全黑，但 GRAM 仍可写入。写完后 `Display On (0x29)` 瞬间呈现。

```cpp
tft.writeCommand(0x28);  // 屏幕全黑，GRAM 可写
tft.fillScreen(BLACK);
tft.drawString(...);
spr.pushSprite(0, 20);
tft.writeCommand(0x29);  // 新画面瞬间完整呈现
```

**适用**：静态一次性画面切换（Menu、IMG Browser 翻页）。

**不适用**：连续动画（Marquee 滚动 33fps、GIF 播放）。每帧都黑屏 → 黑屏时间占比过高 → 严重闪烁。

| 场景 | `0x28`/`0x29` 效果 |
|------|:---:|
| Menu 选项切换 | ✅ 完美 |
| IMG 翻页 | ✅ 完美 |
| Marquee 滚动 | ❌ 每帧黑屏 75% 时间 |
| GIF 播放 | ❌ 快速时黑屏 30% |

**结论**：该方案在静态场景完美，但无法统一处理所有场景。

---

### Phase 4: 全帧后缓冲（最终方案）

**做法**：创建一个与屏幕等大的 `LGFX_Sprite`（320×240×2=150KB PSRAM），所有绘制操作**全部写入这个后缓冲**，完成后一次性 `pushSprite` 推到屏幕。

```
                ┌──────────────┐
   所有绘制 ───→│  backbuffer  │──→ pushSprite(0,0) ──→ TFT
   (Menu/IMG/  │  (320×240)   │     一次性 SPI DMA
    Marquee/   │   PSRAM 150KB │
    GIF/Popup) │              │
                └──────────────┘
```

**关键洞察**：屏幕**始终显示上一帧的完整画面**，载入下一帧期间不做任何屏幕操作。下一帧在 PSRAM 中完全合成后，才一次性推屏。用户永远看不到中间态。

---

## 3. 实现细节

### 3.1 初始化

```cpp
// 全局变量
static LGFX tft;
static LGFX_Sprite backbuffer(&tft);  // 引用 tft 以继承色深等配置

// app_main() 中
tft.init();
tft.setRotation(1);
tft.setBrightness(255);

backbuffer.setPsram(true);             // 从 PSRAM 分配
backbuffer.createSprite(320, 240);     // 150KB
```

### 3.2 各函数改造模板

**静态画面**（Menu、IMG 翻页）：
```cpp
static void draw_xxx() {
    // 所有绘制走 backbuffer
    backbuffer.fillScreen(COLOR_BLACK);
    backbuffer.drawString(...);
    backbuffer.fillRect(...);
    // ...

    // 一次性推屏
    tft.startWrite();
    backbuffer.pushSprite(&tft, 0, 0);
    tft.endWrite();
    sync_button_state();
}
```

**连续动画**（Marquee、GIF）：
```cpp
static void draw_xxx_frame() {
    // 帧内容绘入后缓冲
    backbuffer.fillRect(0, 0, 320, TOP_H, COLOR_BLACK);
    backbuffer.drawString(...);
    backbuffer.pushImage(..., raw_data);      // 或 pushSprite
    // ...

    // 一次性推屏
    tft.startWrite();
    backbuffer.pushSprite(&tft, 0, 0);
    tft.endWrite();
    sync_button_state();
}
```

**弹窗函数**（被上述函数调用）：
```cpp
static void draw_xxx_popup() {
    // 直接绘入 backbuffer（调用者已在构建后缓冲帧）
    backbuffer.fillRect(PX, PY, PW, PH, 0x2104);
    backbuffer.drawRect(...);
    backbuffer.drawString(...);
    // 无需 pushSprite — 由调用者在帧末尾统一推屏
}
```

**错误画面**（独立 push）：
```cpp
// 错误路径需独立推屏
backbuffer.fillScreen(COLOR_BLACK);
backbuffer.drawString("Error!", 160, 120);
tft.startWrite();
backbuffer.pushSprite(&tft, 0, 0);
tft.endWrite();
```

### 3.3 注意事项

| 要点 | 说明 |
|------|------|
| **内存** | 150KB PSRAM，相比 8MB 总容量可忽略 |
| **SPI 流量** | 每帧固定 150KB pushSprite。GIF 原方案 45KB/帧，增 ~105KB；Marquee 原方案 300KB/帧，减半至 150KB |
| **直接 tft 调用** | 严禁！所有 `tft.fill/draw/setText/pushImage` 必须改为 `backbuffer.xxx` |
| **tft 允许的操作** | 仅 `startWrite/endWrite`（包裹 pushSprite）、`init/setRotation/setBrightness/width/height` |
| **必须删除** | `0x28`/`0x29` 命令（不再需要 ST7789 显存控制） |
| **必须删除** | 临时离屏 `LGFX_Sprite spr`（IMG Browser 中原有的部分合成） |

---

## 4. 各场景 SPI 流量对比

| 场景 | 原方案 SPI/帧 | 后缓冲 SPI/帧 | 变化 |
|------|:---:|:---:|:---:|
| Menu 重绘 | ~155KB (fillScreen+元素) | 150KB (pushSprite) | ≈持平 |
| IMG 翻页 | ~130KB (sprite+文字) + 0x28黑屏 | 150KB | 稍增，无黑屏 |
| Marquee 滚动 | 300KB (pushImage×2) | 150KB | **减半** |
| GIF 播放 | ~45KB (pushSprite 200×200) | 150KB | +105KB |

> GIF 增速最坏情况(75ms/帧)：150KB @ 80MHz SPI ≈ 15ms，占帧时 20%，可接受。

---

## 5. 设计原则总结

1. **屏幕永远显示完整帧**：不展示中间绘制过程
2. **PSRAM 做画布，SPI 做搬运**：绘制≠显示，二者解耦
3. **统一＞分散**：一个后缓冲覆盖所有场景，消除场景间的方案差异
4. **简单＞精巧**：全帧推屏 150KB 看似"浪费"，但代码简洁性和体验一致性远超局部优化
