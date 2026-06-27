# 🎤 ASR 语音识别

> 录音 → WiFi 上传 → PC 服务器 (Whisper + DeepSeek) → JSON 返回 → 显示

---

## ESP32 端：4 阶段状态机

```c
// main.cpp handle_asr()
s_asr_phase: 0=idle  1=preparing  2=recording  3=uploading
```

| Phase | 说明 |
|-------|------|
| 0 | 空闲：编辑文本、L/R 移动光标（自动滚动）、DOWN 删除、START 开始录音 |
| 1 | 准备中：显示 "Preparing..."，等待 `record_is_capturing()` 为 true |
| 2 | 录音中：显示倒计时。按 BACK 启动 300ms 缓冲（不立即停，保留尾音）。自然超时自动停止 |
| 3 | 上传中：HTTP POST WAV 到服务器，等待 `asr_result_ready()`。结果追加到文本缓冲 |

---

## HTTP 上传 (asr.cpp)

```c
// 手动控制 HTTP 流程（不用 esp_http_client_perform）
esp_http_client_open  →  esp_http_client_write  →  esp_http_client_fetch_headers  →  esp_http_client_read
```

服务器地址在 `asr.h` 的 `ASR_SERVER_URL` 宏中，按实际环境修改。

**已知内存泄漏（已修复）**：`upload_task` 的 `strdup(path)` 必须在 `done:` 标签处 `free((void*)path)`。

---

## 文本显示：像素宽度感知换行

中文/英文混合文本的换行不能按字符数（`CHARS=18`），必须按像素宽度：

```c
// display.cpp draw_asr_text()
const int UNITS_PER_LINE = 36;  // 288px / 8px = 36 半宽单位
// ASCII: 1 单位 (8px), CJK: 2 单位 (16px)
```

光标移动后自动 clamp `scroll_line` 使光标始终可见，无需额外的上下滚动按键。

---

## PC 服务器端 (server/asr_server.py)

### Whisper 配置

```python
WHISPER_MODEL.transcribe(tmp_path, language=None, beam_size=5)
```

**`language=None`** 是必须的——自动检测中英文。不要设 `language="zh"`，否则英文识别为乱码。

回退逻辑：检测到非 zh/en 语言时用 `language="zh"` 重识别。

### DeepSeek AI 纠错

```python
system_prompt = (
    "你是语音识别纠错助手。根据输入语言自动选择纠错策略：\n"
    "- 中文：修正错别字、语法、同音词\n"
    "- 英文：修正 spelling、grammar、homophones\n"
    "- 中英混合：分别按各自规则修正\n"
    "严格只输出纠错后的纯文本，不加任何解释。"
)
```

**中文 only 的 prompt 会导致英文被误处理**。必须用双语 prompt。

### zhconv

`convert(text, "zh-cn")` 繁→简转换。对英文是 no-op，不会破坏英文文本。

### Debug WAV

识别完成后保存到 `debug_wav/{时间戳}_{序号}_{语言}_{大小}.wav`，方便分析音频质量。

### 依赖

```
faster-whisper
zhconv
requests
```

---

## 按钮映射

| 按键 | Phase 0 | Phase 1 | Phase 2 | Phase 3 |
|------|---------|---------|---------|---------|
| START | 开始录音 | — | — | — |
| BACK | 回菜单 | 取消录音 | 缓冲停止 | 取消等待 |
| LEFT | 光标左移 | — | — | — |
| RIGHT | 光标右移 | — | — | — |
| DOWN | 删除字符 | — | — | — |

⚠️ DOWN 键同时用作删除和滚动会导致冲突。只保留删除功能，滚动由光标移动自动驱动。

---

## 相关文件

- `main/main.cpp` — `handle_asr()` 状态机
- `components/asr/asr.cpp` — HTTP 上传 + 文本缓冲
- `components/display/display.cpp` — `draw_asr_text()` / `draw_asr_preparing()`
- `server/asr_server.py` — Whisper + DeepSeek
- `server/config.py` — DeepSeek API Key（gitignore）
