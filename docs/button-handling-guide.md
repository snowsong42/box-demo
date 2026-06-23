# 按钮交互系统指南

> 两层架构：边沿检测（导航）+ DIJI-NES 时间门控（弹窗）。新增按钮或操作时照此模板实现。

---

## 1. 架构总览

```
┌─────────────────────────────────────────────────┐
│                  main loop                       │
│  ┌──────────────┐    ┌──────────────────────┐   │
│  │ read_buttons │    │ gpio_get_level       │   │
│  │ (边沿检测)    │    │ (电平直读)            │   │
│  │              │    │                      │   │
│  │ 用于:        │    │ 用于:                 │   │
│  │ · 菜单导航    │    │ · 弹窗确认/取消        │   │
│  │ · 图片翻页    │    │ · 弹窗 armed 判断     │   │
│  │ · 速度调节    │    │                      │   │
│  │ · 打开弹窗    │    │                      │   │
│  └──────┬───────┘    └──────────┬───────────┘   │
│         │                       │               │
│         ▼                       ▼               │
│  ┌──────────────────────────────────────────┐   │
│  │            handle_*()                     │   │
│  │  弹窗路径: gpio_get_level + 200ms 冷却    │   │
│  │  导航路径: switch(btn) 边沿事件           │   │
│  └──────────────────────────────────────────┘   │
│                      │                          │
│                      ▼                          │
│  ┌──────────────────────────────────────────┐   │
│  │            draw_*()                       │   │
│  │  全部绘制到 backbuffer → 一次性 pushSprite │   │
│  └──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
```

**核心原则**：
- **导航/翻页/调速** — 用 `read_buttons()` 边沿事件。轻触即响应，按住不重复。
- **弹窗确认/取消** — 用 `gpio_get_level()` 直接读电平。不依赖边沿（长耗时绘制期间边沿会丢失）。
- **弹窗保护** — 打开弹窗的按键必须先松手，才能用来确认/取消。防止瞬间连发。

---

## 2. 底层：按钮 GPIO 配置

### 2.1 引脚定义

```cpp
// 按键 GPIO
#define BTN_UP     GPIO_NUM_17
#define BTN_DOWN   GPIO_NUM_3
#define BTN_LEFT   GPIO_NUM_8
#define BTN_RIGHT  GPIO_NUM_18

// 按键返回值 (边沿事件用)
#define BTN_NONE  0
#define BTN_U     1
#define BTN_D     2
#define BTN_L     3
#define BTN_R     4
```

### 2.2 初始化

所有按钮统一配置为 `INPUT_PULLUP`，按下列低电平（GND），松开即高电平（内部 ~45kΩ 上拉）。不使用 GPIO 中断——全程轮询。

```cpp
static void init_buttons() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pin_bit_mask = (1ULL << BTN_UP) | (1ULL << BTN_DOWN)
                         | (1ULL << BTN_LEFT) | (1ULL << BTN_RIGHT);
    gpio_config(&io_conf);
}
```

### 2.3 新增按钮

如果要加第 5 个键（如 OK 键接 GPIO9）：

```cpp
// 1. 加宏
#define BTN_OK     GPIO_NUM_9
#define BTN_K      5   // 新返回值

// 2. io_conf.pin_bit_mask 加上 (1ULL << BTN_OK)

// 3. read_buttons() 中加一行：
if (gpio_get_level(BTN_OK) == 0) curr = BTN_K;
```

> ⚠️ `read_buttons()` 中的 `if` 是**独立判断**（不是 `else if`），多个键同时按下时，最后检查的键优先级最高。如果你的应用需要严格优先级（如 UP > DOWN > LEFT > RIGHT），改用 `else if` 链。

---

## 3. 中层：边沿检测（导航用）

### 3.1 `read_buttons()` — 边沿事件读取

```cpp
static int prev_btn = BTN_NONE;  // 全局状态，追踪上一帧按下的键

/// 仅在"松→按"的下降沿返回事件。按住不重复。长耗时绘制后无副作用。
static int read_buttons() {
    int curr = BTN_NONE;
    if (gpio_get_level(BTN_UP) == 0)    curr = BTN_U;
    if (gpio_get_level(BTN_DOWN) == 0)  curr = BTN_D;
    if (gpio_get_level(BTN_LEFT) == 0)  curr = BTN_L;
    if (gpio_get_level(BTN_RIGHT) == 0) curr = BTN_R;

    int event = (curr != BTN_NONE && curr != prev_btn) ? curr : BTN_NONE;
    prev_btn = curr;
    return event;
}
```

**特性**：
| 场景 | 行为 |
|------|------|
| 按下瞬间 | 返回按键值（如 `BTN_U`） |
| 持续按住 | 返回 `BTN_NONE`（不重复触发） |
| 松开后再按 | 再次返回按键值 |
| 长耗时绘制后 | 无副作用——不调用 `sync_button_state` |

### 3.2 在 handler 中使用

```cpp
static void handle_img(int btn) {
    // ... 弹窗处理（见第 4 节）...

    // 边沿事件驱动导航
    switch (btn) {
        case BTN_L:
            img_index = (img_index - 1 + img_count) % img_count;
            draw_img_browser();
            break;
        case BTN_R:
            img_index = (img_index + 1) % img_count;
            draw_img_browser();
            break;
        case BTN_D:
            img_exit_popup = true;
            img_exit_armed = true;
            draw_img_browser();
            break;
    }
}
```

---

## 4. 中层：DIJI-NES 弹窗模式（确认/取消用）

### 4.1 为什么弹窗不能用边沿检测

假设用户在图片浏览器中按 DOWN 想打开退出弹窗。此时 `draw_img_browser()` 正在加载 PNG（100-500ms）。等绘制完毕，用户的 DOWN 按键可能已经松开了——`read_buttons()` 根本看不到这次边沿。

**弹窗必须直接读 `gpio_get_level()`**——只要按键还按着，就能读到。

### 4.2 三段式弹窗模板

DIJI-NES 弹窗模式 = **等松手 → 冷却门控 → 电平行动**。所有弹窗（菜单确认、子页面退出）使用完全相同的模板，只需要改变按键和动作。

```cpp
// ===== 状态变量（每个弹窗一套） =====
static bool xxx_popup = false;        // 弹窗是否显示
static bool xxx_armed = false;        // true=等待松手, false=可确认

// ===== 冷却计时器（全局共享） =====
static int64_t last_action_time = 0;
// 200ms 门控：防双击

// ===== 打开弹窗（在 handler 的导航路径中） =====
case BTN_D:   // 或 BTN_R，视具体弹窗而定
    ESP_LOGI(TAG, "POPUP: exit XXX?");
    xxx_popup = true;
    xxx_armed = true;    // ← 必须等松手
    draw_xxx_frame();    // ← 立即绘制弹窗
    break;               // 或 return

// ===== 处理弹窗（在 handler 最顶部，早于导航 switch） =====
if (xxx_popup) {
    // --- 第 1 段：读电平 ---
    bool yes = (gpio_get_level(BTN_YES) == 0);  // 确认键
    bool no  = (gpio_get_level(BTN_NO)  == 0);  // 取消键

    // --- 第 2 段：等松手 ---
    if (!yes && !no) xxx_armed = false;
    if (xxx_armed) { draw_xxx_frame(); return; }

    // --- 第 3 段：冷却门控 + 行动 ---
    int64_t now = esp_timer_get_time() / 1000;
    if (yes && (now - last_action_time >= 200)) {
        last_action_time = now;
        // === 确认动作 ===
        ESP_LOGI(TAG, "EXIT: XXX -> MENU");
        xxx_popup = false;
        xxx_need_init = true;   // ← 重进入时重新初始化
        // ... 清理资源（free、i2s_disable 等）...
        current_state = STATE_MENU;
        draw_menu();
    } else if (no && (now - last_action_time >= 200)) {
        last_action_time = now;
        // === 取消动作 ===
        ESP_LOGI(TAG, "CANCEL");
        xxx_popup = false;
        draw_xxx_frame();       // 回到子页面
    } else {
        draw_xxx_frame();       // 保持弹窗显示 + 画面刷新
    }
    return;
}
```

### 4.3 三个关键变量

| 变量 | 作用 |
|------|------|
| `xxx_popup` | 弹窗是否激活。`draw_xxx_frame()` 检测此标志叠加绘制弹窗 |
| `xxx_armed` | `true` = 打开弹窗的按键还没松手，禁止任何确认/取消操作 |
| `last_action_time` | 全局共享。任何弹窗确认/取消后重置，200ms 内禁止再次行动 |

### 4.4 等松手（`xxx_armed`）的工作原理

```
用户按下 DOWN（打开弹窗）
  → xxx_popup = true
  → xxx_armed = true      ← "武器已上膛"，必须松手
  → draw_xxx_frame() 显示弹窗

每帧循环：
  gpio_get_level(DOWN) → 1（还按着）
  → xxx_armed 保持 true → 只绘制，不处理确认/取消

用户松开 DOWN：
  gpio_get_level(DOWN) → 0, gpio_get_level(UP) → 0
  → !yes && !no → xxx_armed = false  ← "保险已解除"

用户按下 DOWN（确认退出）：
  yes=true, xxx_armed=false, 冷却通过
  → 执行退出动作 ✅
```

### 4.5 完整示例：菜单确认弹窗

```cpp
// ---- 状态变量 ----
static bool menu_popup = false;
static bool menu_popup_armed = false;

// ---- 打开弹窗 (handle_menu 中) ----
case BTN_R:
    ESP_LOGI(TAG, "POPUP: enter %s?", menu_items[menu_selection]);
    menu_popup = true;
    menu_popup_armed = true;
    draw_menu();
    break;

// ---- 处理弹窗 (handle_menu 最顶部) ----
if (menu_popup) {
    bool yes = (gpio_get_level(BTN_RIGHT) == 0);
    bool no  = (gpio_get_level(BTN_LEFT)  == 0);
    if (!yes && !no) menu_popup_armed = false;
    if (menu_popup_armed) { draw_menu(); return; }

    int64_t now = esp_timer_get_time() / 1000;
    if (yes && (now - last_action_time >= 200)) {
        last_action_time = now;
        ESP_LOGI(TAG, "ENTER: %s", menu_items[menu_selection]);
        menu_popup = false;
        current_state = (AppState)(STATE_IMG + menu_selection);
        draw_menu();
    } else if (no && (now - last_action_time >= 200)) {
        last_action_time = now;
        ESP_LOGI(TAG, "CANCEL");
        menu_popup = false;
        draw_menu();
    } else {
        draw_menu();
    }
    return;
}
```

### 4.6 完整示例：子页面退出弹窗

```cpp
// ---- 状态变量 (每个子页面一套) ----
static bool img_exit_popup = false;
static bool img_exit_armed = false;

// ---- 打开弹窗 (handle_img 中) ----
case BTN_D:
    ESP_LOGI(TAG, "POPUP: exit IMG?");
    img_exit_popup = true;
    img_exit_armed = true;
    draw_img_browser();
    break;

// ---- 处理弹窗 (handle_img 最顶部, 在 init 和导航之间) ----
if (img_exit_popup) {
    bool yes = (gpio_get_level(BTN_DOWN) == 0);
    bool no  = (gpio_get_level(BTN_UP)   == 0);
    if (!yes && !no) img_exit_armed = false;
    if (img_exit_armed) { draw_img_browser(); return; }

    int64_t now = esp_timer_get_time() / 1000;
    if (yes && (now - last_action_time >= 200)) {
        last_action_time = now;
        ESP_LOGI(TAG, "EXIT: IMG -> MENU");
        img_exit_popup = false;
        img_need_init = true;        // ← 重进入时重新加载
        audio_running = false;
        if (audio_task_handle) { vTaskDelay(pdMS_TO_TICKS(100)); }
        i2s_channel_disable(tx_chan);
        current_state = STATE_MENU;
        draw_menu();
    } else if (no && (now - last_action_time >= 200)) {
        last_action_time = now;
        ESP_LOGI(TAG, "CANCEL");
        img_exit_popup = false;
        draw_img_browser();
    } else {
        draw_img_browser();
    }
    return;
}
```

---

## 5. 上层：状态机路由

### 5.1 状态枚举

```cpp
enum AppState {
    STATE_MENU,       // 0 = 主菜单
    STATE_IMG,        // 1 = 图片浏览器
    STATE_MARQUEE,    // 2 = 图片走马灯
    STATE_GIF         // 3 = GIF 播放器
};
static AppState current_state = STATE_MENU;
```

### 5.2 主循环（事件驱动 + 持续渲染）

```cpp
while (1) {
    int btn = read_buttons();

    switch (current_state) {
        case STATE_MENU:
        case STATE_IMG:
            // 事件驱动：只在有操作时重绘
            if (current_state == STATE_MENU) handle_menu(btn);
            else handle_img(btn);
            vTaskDelay(pdMS_TO_TICKS(10));
            break;

        case STATE_MARQUEE:
            // 持续渲染：每帧推进滚动
            handle_marquee(btn);
            vTaskDelay(pdMS_TO_TICKS(30));
            break;

        case STATE_GIF:
            // 持续渲染：每帧检查是否需要推进
            handle_gif(btn);
            vTaskDelay(pdMS_TO_TICKS(10));
            break;
    }
}
```

**关键设计**：
- `STATE_MENU` / `STATE_IMG` 共享 case（fall-through），因为它们都是事件驱动模式
- `STATE_MARQUEE` / `STATE_GIF` 独立 case，因为它们需要每帧持续渲染动画
- handler **始终被调用**（不判断 `btn != BTN_NONE`），确保状态转换后的首次 init 不丢失
- 延迟根据渲染需求调整：菜单/图片 10ms，走马灯 30ms，GIF 10ms

### 5.3 状态转换三原则

向菜单添加新功能（如 `STATE_PLAYER`）时，遵循三条规则：

| # | 原则 | 说明 |
|---|------|------|
| 1 | **进入时画 Loading** | `*_need_init` 路径先显示 "Loading..."，再加载资源 |
| 2 | **退出时重置 init** | `*_need_init = true` — 保证下次进入重新初始化 |
| 3 | **资源成对管理** | `malloc` 对应 `free`；`createSprite` 对应 `deleteSprite`；`i2s_channel_enable` 对应 `disable` |

### 5.4 handler 函数签名约定

```cpp
static void handle_xxx(int btn);
// btn: read_buttons() 返回的边沿事件，BTN_NONE 表示无事件
// handler 内部：
//   1. 检查 *_need_init → 加载资源
//   2. 检查弹窗 → DIJI-NES 三段式
//   3. switch(btn) → 边沿事件驱动导航
```

---

## 6. 资源生命周期

### 6.1 `*_need_init` 模式

每个子功能有一个 `static bool xxx_need_init = true` 标志：

```cpp
static void handle_img(int btn) {
    if (img_need_init) {
        // 1. 显示 Loading
        backbuffer.fillScreen(COLOR_BLACK);
        backbuffer.drawString("Loading...", 160, 120);
        tft.startWrite(); backbuffer.pushSprite(&tft, 0, 0); tft.endWrite();

        // 2. 加载资源
        img_count = detect_img_count();
        img_index = 0;
        img_exit_popup = false;
        img_need_init = false;

        // 3. 启动音频（如果需要）
        if (!audio_running) {
            audio_running = true;
            i2s_channel_enable(tx_chan);
            xTaskCreate(audio_playback_task, "audio", 4096, NULL, 1, &audio_task_handle);
        }

        // 4. 绘制首帧
        draw_img_browser();
        return;
    }
    // ... 正常处理 ...
}
```

### 6.2 退出时重置

```cpp
// 弹窗确认退出时必须做的三件事：
marquee_exit_popup = false;
marquee_need_init = true;    // ← 下次进入重新加载
audio_running = false;
if (audio_task_handle) { vTaskDelay(pdMS_TO_TICKS(100)); }
i2s_channel_disable(tx_chan);
if (marquee_raw) { heap_caps_free(marquee_raw); marquee_raw = nullptr; }
current_state = STATE_MENU;
draw_menu();
```

### 6.3 推屏规范

所有 draw 函数在末尾统一推屏，不调用 `sync_button_state`：

```cpp
static void draw_img_browser() {
    // 1. 全部绘制到 backbuffer
    backbuffer.fillScreen(COLOR_BLACK);
    backbuffer.drawString(...);
    backbuffer.drawPng(...);
    // ...

    // 2. 一次性推屏（无闪烁、无撕裂）
    tft.startWrite();
    backbuffer.pushSprite(&tft, 0, 0);
    tft.endWrite();
    // 不再调用 sync_button_state
}
```

---

## 7. 添加新功能的检查清单

### Step 1：定义状态

```cpp
enum AppState {
    STATE_MENU,
    STATE_IMG,
    STATE_MARQUEE,
    STATE_GIF,
    STATE_NEW_FEATURE,   // ← 新增
};
```

### Step 2：声明状态变量

```cpp
static bool new_feature_need_init = true;      // 首次进入标志
static bool new_feature_exit_popup = false;     // 退出弹窗
static bool new_feature_exit_armed = false;     // 弹窗等松手
// ... 功能专用变量 ...
```

### Step 3：写 `draw_xxx()` 和 `handle_xxx()`

```cpp
static void draw_new_feature() {
    backbuffer.fillScreen(COLOR_BLACK);
    // ... 绘制 ...
    if (new_feature_exit_popup) draw_exit_popup();  // 弹窗叠加
    tft.startWrite();
    backbuffer.pushSprite(&tft, 0, 0);
    tft.endWrite();
}

static void handle_new_feature(int btn) {
    if (new_feature_need_init) {
        // 显示 Loading + 加载资源 + draw + return
    }
    if (new_feature_exit_popup) {
        // DIJI-NES 三段式弹窗（见 §4.2 模板）
    }
    switch (btn) {
        // 边沿事件驱动导航
    }
}
```

### Step 4：补主循环

```cpp
switch (current_state) {
    case STATE_MENU:
    case STATE_IMG:
    case STATE_NEW_FEATURE:   // ← 加入事件驱动组
        if (current_state == STATE_MENU) handle_menu(btn);
        else if (current_state == STATE_IMG) handle_img(btn);
        else handle_new_feature(btn);
        vTaskDelay(pdMS_TO_TICKS(10));
        break;
    // ...
}
```

如果是持续渲染型（类似走马灯/GIF），则独立 case：
```cpp
    case STATE_NEW_FEATURE:
        handle_new_feature(btn);
        vTaskDelay(pdMS_TO_TICKS(XX));
        break;
```

### Step 5：菜单加条目（如需要确认弹窗）

```cpp
static const char* menu_items[] = {
    "IMG Browser",
    "IMG Marquee",
    "GIF Player",
    "New Feature"     // ← 新增
};
#define MENU_COUNT 4   // ← 更新数量
```

弹窗确认进入复用 §4.5 模板，只需将 `STATE_IMG + menu_selection` 的映射改对（`menu_selection=3` 应映射到 `STATE_NEW_FEATURE`）。

---

## 8. 反面教材：这些模式已废弃

以下是本次重构前存在的反模式，**在新代码中切勿使用**。

| 反模式 | 表现 | 后果 | 正确做法 |
|--------|------|------|---------|
| **`btn != BTN_NONE` 门控** | `if (btn != BTN_NONE) handle_*(btn);` | 状态转换后首次 init 被跳过（按钮按住时 event=BTN_NONE） | 始终调用 handler，不判断 btn |
| **忘记 `*_need_init = true`** | 退出时不重置 | 重进入时资源已释放，pushImage(nullptr) 导致画面叠加 | 退出时三件套：popup=false + need_init=true + 释放资源 |
| **边沿+电平双层弹窗** | 弹窗同时走 `btn==X` 和 `held==X && timer>400ms` | 打开弹窗的按钮 400ms 后自动确认，用户无感知 | DIJI-NES 三段式（§4.2） |
| **`sync_button_state()`** | 每次 draw 后调用 | 长绘制期间的按键被"吃掉"（设为 prev_btn），下次 read_buttons 无事件 | 删掉。弹窗用 gpio_get_level 直接读 |
| **弹窗按住不刷新** | `if (held==NONE) timer=0; else if (...) {...} return;` | 弹窗+按住期间画面冻结 | 所有路径末尾都调 draw |
| **i2s_disable 紧接 audio=false** | 设标志后立即关通道 | 音频任务阻塞在 write 中 → disable 失败 | 先等任务退出（vTaskDelay 100ms），再 disable |

---

## 9. 参考

- 本项目 DIJI-NES 参考：`docs/diji-nes-reference.md`
- 上游 DIJI-NES 实现：`DIJI-NES-main/src/main.cpp` — 时间门控防抖 + "等松开" 模式
- 专业按钮库参考：`brooksbUWO/Debounce` (16 位移位寄存器消抖)、`AdrianPietrzak1998/button_library` (完整 FSM)
- 应用架构总览：`docs/app-architecture.md`
