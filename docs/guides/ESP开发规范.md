
# ESP-IDF 项目开发规范

> **适用范围**：box-demo 及基于乐鑫 ESP-IDF 框架的所有 ESP32 系列项目。
> **参考来源**：[Espressif ESP-IDF 编码风格指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/contribute/style-guide.html)
> **版本**：v1.1 | **最后更新**：2025-07-03

> [^1]: `g_` 前缀是项目扩展规范。官方风格指南只定义了 `s_` 前缀用于 static 变量。

---

## 目录

1. [项目仓库结构规范](#1-项目仓库结构规范)
2. [Git 分支与提交规范](#2-git-分支与提交规范)
3. [C 语言代码风格](#3-c-语言代码风格)
4. [命名规范](#4-命名规范)
5. [头文件规范](#5-头文件规范)
6. [注释与文档规范](#6-注释与文档规范)
7. [错误处理规范](#7-错误处理规范)
8. [FreeRTOS 使用规范](#8-freertos-使用规范)
9. [日志规范](#9-日志规范)
10. [SDK 配置管理](#10-sdk-配置管理)
11. [CMake 代码风格](#11-cmake-代码风格)
12. [CI/CD 与质量保障](#12-cicd-与质量保障)
13. [附录：配置示例文件](#13-附录配置示例文件)

---

## 1. 项目仓库结构规范

### 1.1 标准 ESP-IDF 项目目录
```

project-name/
├── CMakeLists.txt                # 顶层 CMake 构建入口（必需）
├── sdkconfig.defaults            # 默认 SDK 配置（提交到仓库）
├── sdkconfig                     # 本地 SDK 配置（不提交）
├── partitions.csv                # 分区表定义
  -Dependencies.lock             # 组件依赖锁定文件（提交到仓库）
├── README.md                     # 项目说明
├── LICENSE                       # 许可证
├── .gitignore                    # Git 忽略规则
├── .clang-format                 # 代码格式化配置 ⚠️ 官方使用 astyle，clang-format 为项目扩展
├── .editorconfig                 # 编辑器统一配置
│
├── main/                         # 主应用程序
│   ├── CMakeLists.txt            # idf_component_register(...)
│   ├── main.c                    # app_main() 入口
│   ├── idf_component.yml         # 组件依赖声明
│   └── include/                  # 主程序私有头文件
│
├── components/                   # 自定义组件
│   └── my_component/
│       ├── CMakeLists.txt
│       ├── Kconfig               # menuconfig 配置选项
│       ├── src/
│       └── include/
│
├── managed_components/           # 组件管理器自动下载（不提交）
│
├── docs/                         # 项目文档
│   ├── architecture.md
│   └── guides/
│
├── tests/                        # 测试代码
│   ├── unit_tests/
│   └── hardware_tests/
│
├── scripts/                      # 辅助脚本
├── configs/                      # 多环境 SDK 配置
│   ├── release/
│   └── debug/
│
├── hardware/                     # 硬件设计文件（可选）
│
├── .github/                      # GitHub Actions CI
│   └── workflows/
│       └── build.yml
│
└── build/                        # 构建输出（不提交）

```

### 1.2 `.gitignore` 推荐配置

```gitignore
# ESP-IDF 构建产物
build/
sdkconfig
sdkconfig.old
*.pyc
__pycache__/

# 组件管理器（自动下载，不提交）
managed_components/

# IDE / 编辑器（按需选择是否提交 .vscode/）
.idea/
*.swp
*.swo
*~

# 操作系统
.DS_Store
Thumbs.db

# 编译中间产物
*.o
*.d
*.elf
*.map
*.bin

# 环境变量
.env
.env.local
```

### 1.3 文件编码

| 规则       | 说明                                  |
| ---------- | ------------------------------------- |
| 源文件编码 | **UTF-8**（无 BOM）             |
| 行尾符     | **LF**（`\n`），不要使用 CRLF |
| 文件末尾   | 必须以一个空行结尾                    |

---

## 2. Git 分支与提交规范

### 2.1 分支策略（推荐 GitHub Flow）

| 分支               | 用途         | 说明                               |
| ------------------ | ------------ | ---------------------------------- |
| `main`           | 稳定发布分支 | 始终保持可发布状态，只接受 PR 合并 |
| `develop`        | 开发主分支   | 日常集成（小团队可选）             |
| `feature/<name>` | 功能开发     | 从`main`（或 `develop`）分出   |
| `fix/<name>`     | 缺陷修复     | 修复完成后合并回`main`           |
| `release/vX.Y.Z` | 发布候选     | 冻结功能，只修 bug                 |

### 2.2 Commit Message 规范（Conventional Commits）

```
<type>(<scope>): <简短描述>

<详细描述（可选）>

<关联 issue（可选）>
```

**类型定义：**

| 类型         | 说明      | 示例                                     |
| ------------ | --------- | ---------------------------------------- |
| `feat`     | 新功能    | `feat(wifi): 添加 WPS 一键配网功能`    |
| `fix`      | 缺陷修复  | `fix(spi): 修复 SPI 总线时钟配置错误`  |
| `docs`     | 文档变更  | `docs: 更新 API 参考文档`              |
| `refactor` | 代码重构  | `refactor(led): 重构 LED 驱动抽象层`   |
| `perf`     | 性能优化  | `perf(http): 优化 HTTP 解析缓冲区使用` |
| `test`     | 测试相关  | `test: 添加 OTA 升级集成测试`          |
| `chore`    | 构建/工具 | `chore: 升级 ESP-IDF 到 v5.3`          |

### 2.3 版本号规范（SemVer）

格式：`MAJOR.MINOR.PATCH`

- **MAJOR**：不兼容的 API / 硬件变更
- **MINOR**：向后兼容的新功能
- **PATCH**：向后兼容的 bug 修复

发布时打 Tag：

```bash
git tag -a v1.2.0 -m "Release v1.2.0: 新增 OTA 功能"
git push origin v1.2.0
```

---

## 3. C 语言代码风格

### 3.1 缩进与空格

| 规则       | 值                                 |
| ---------- | ---------------------------------- |
| 缩进单位   | **4 个空格**（禁止使用 Tab） |
| 续行缩进   | 与上一行对齐，或额外缩进 4 空格    |
| 运算符空格 | 二元运算符两侧各加一个空格         |

### 3.2 行宽限制

- **推荐**：120 字符以内
- **最大**：不超过 150 字符
- 超过行宽时，在运算符处断行

### 3.3 大括号风格（K&R 变体）

```c
// ✅ 正确 — 函数：左大括号另起一行
static void my_function(void)
{
    // ...
}

// ✅ 正确 — 控制语句：左大括号在同一行
if (condition) {
    do_something();
} else {
    do_other();
}

// ✅ 正确 — 单行语句也必须使用大括号
if (condition) {
    return ESP_OK;
}

// ✅ 正确 — do-while
do {
    count--;
} while (count > 0);
```

### 3.4 空行与垂直空白

- 函数之间保留一个空行
- 逻辑块之间可用空行分隔
- 文件末尾保留一个空行

### 3.5 指针声明

```c
// ✅ 正确 — 星号紧贴变量名
char *name;
int *ptr;

// ❌ 错误
char* name;
char * name;
```

### 3.6 类型定义

```c
// ✅ 结构体使用 typedef
typedef struct {
    int x;
    int y;
} point_t;

// ✅ 枚举使用 typedef + 模块名前缀（避免命名冲突）
typedef enum {
    MODULE_FOO_ONE,
    MODULE_FOO_TWO,
    MODULE_FOO_THREE,
} module_foo_t;
```

---

## 4. 命名规范

### 4.1 命名总览（ESP-IDF 官方风格）

| 类别          | 命名风格                     | 示例                                    |
| ------------- | ---------------------------- | --------------------------------------- |
| 函数名        | `snake_case`               | `wifi_init_sta()`                     |
| 局部变量      | `snake_case`               | `int retry_count;`                    |
| 全局变量      | `g_` 前缀 + `snake_case`  [^1] | `static int g_connection_state;`      |
| 静态变量      | `s_` 前缀 + `snake_case` | `static bool s_is_initialized;`       |
| 结构体/联合体 | `snake_case` + `_t` 后缀 | `wifi_config_t`                       |
| 枚举类型      | `snake_case` + `_t` 后缀 | `led_mode_t`                          |
| 枚举值        | `UPPER_SNAKE_CASE`         | `LED_MODE_BLINK`                      |
| 宏 / 常量     | `UPPER_SNAKE_CASE`         | `#define MAX_RETRIES 5`               |
| 类型定义      | `snake_case` + `_t` 后缀 | `typedef struct { ... } my_struct_t;` |

### 4.2 函数命名

```c
// ✅ 模块_动作_对象 模式
esp_err_t wifi_init_sta(void);
esp_err_t http_client_send_request(const char *url);
void led_set_brightness(uint8_t level);

// ✅ 布尔返回函数用 is_ / has_ 前缀
bool is_wifi_connected(void);
bool has_pending_data(void);
```

### 4.3 API 前缀

```c
// ESP-IDF 公共 API 使用 esp_ 前缀
esp_err_t esp_wifi_connect(void);

// 项目自定义 API 建议使用项目前缀
esp_err_t box_sensor_read_data(sensor_data_t *data);
```

---

## 5. 头文件规范

### 5.1 头文件保护

**优先使用 `#pragma once`**，并且所有 C 头文件必须包含 `extern "C"` 保护：

```c
// ✅ 推荐 — 完整头文件模板
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 声明放在此处 */

#ifdef __cplusplus
}
#endif
```

```c
// ✅ 备选（兼容旧编译器）
#ifndef MY_HEADER_H
#define MY_HEADER_H
// ...
#endif
```

### 5.2 Include 顺序

遵循从通用到特定的原则，每组之间用空行分隔：

```c
// 1. 标准 C 库
#include <stdio.h>
#include <string.h>

// 2. FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 3. ESP-IDF 驱动 / 组件
#include "esp_log.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

// 4. 项目内头文件
#include "my_component.h"
```

### 5.3 前向声明优先于 `#include`

```c
// ✅ 头文件中尽量用前向声明
typedef struct sensor_data_t sensor_data_t;

void process_data(sensor_data_t *data);

// 只在 .c 文件中 #include 完整定义
```

---

## 6. 注释与文档规范

### 6.1 文件头注释

```c
/*
 * SPDX-FileCopyrightText: 2025 Your Name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file my_component.c
 * @brief 简要描述该文件的功能
 */
```

### 6.2 函数文档（Doxygen 风格）

```c
/**
 * @brief 初始化 Wi-Fi 站模式
 *
 * 该函数配置 ESP32 为 Wi-Fi 客户端，连接到指定 AP。
 * 必须在 nvs_flash_init() 之后调用。
 *
 * @param[in] ssid     Wi-Fi SSID（最大 32 字节）
 * @param[in] password Wi-Fi 密码（最大 64 字节）
 *
 * @return
 *     - ESP_OK   连接成功
 *     - ESP_FAIL 连接超时或配置错误
 *
 * @note 此函数会阻塞直到连接成功或超时。
 */
esp_err_t wifi_init_sta(const char *ssid, const char *password);
```

### 6.3 行内注释

```c
// ✅ 解释"为什么"而非"是什么"
// 此处需要延迟 100ms，等待传感器电源稳定
vTaskDelay(pdMS_TO_TICKS(100));

// ❌ 废话注释
// 延迟 100ms
vTaskDelay(pdMS_TO_TICKS(100));

// ✅ TODO / FIXME 标记
// TODO: 替换为事件驱动方式，避免轮询
// FIXME: 当缓冲区满时会丢失数据
```

---

## 7. 错误处理规范

### 7.1 `assert()` vs 返回值

| 方式 | 适用场景 | 说明 |
|------|----------|------|
| `assert()` | **不可恢复的**内部逻辑错误 | 断言失败 → `abort()` → 严重错误 |
| `esp_err_t` 返回值 | **可恢复的**错误（无效输入等） | 调用方检查返回值并处理 |
| `ESP_ERROR_CHECK()` | 断言 `esp_err_t == ESP_OK` | 失败等同于 `assert()` |

```c
// ✅ assert 用于"绝不该发生"的情况
assert(pointer != NULL);

// ✅ esp_err_t 用于可恢复的外部错误
esp_err_t ret = sensor_read(&data);
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "传感器读取失败，使用默认值");
}
```

> ⚠️ 断言可能被编译优化掉（`CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL`），`assert()` 内的函数不应有副作用。

### 7.2 返回值约定

ESP-IDF 函数统一使用 `esp_err_t` 作为返回值类型：

```c
// ✅ 标准返回模式
esp_err_t my_func(void)
{
    esp_err_t ret = ESP_OK;

    ret = some_operation();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "操作失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
```

### 7.2 使用 `ESP_RETURN_ON_ERROR` 宏

```c
// ✅ 精简的错误返回（ESP-IDF v5.0+）
esp_err_t my_init(void)
{
    ESP_RETURN_ON_ERROR(initialize_step1(), TAG, "step1 failed");
    ESP_RETURN_ON_ERROR(initialize_step2(), TAG, "step2 failed");
    return ESP_OK;
}
```

### 7.3 使用 `ESP_GOTO_ON_ERROR` 进行资源清理

```c
esp_err_t my_complex_func(void)
{
    esp_err_t ret = ESP_OK;
    void *buffer = NULL;

    buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ret = do_work(buffer);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "work failed");

cleanup:
    free(buffer);
    return ret;
}
```

---

## 8. FreeRTOS 使用规范

### 8.1 任务创建

```c
// ✅ 使用静态任务创建（推荐，内存可控）
static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[TASK_STACK_SIZE];

TaskHandle_t task_handle = xTaskCreateStatic(
    my_task_func,
    "my_task",
    TASK_STACK_SIZE,
    NULL,
    TASK_PRIORITY,
    s_task_stack,
    &s_task_tcb
);
```

### 8.2 内存分配

| 场景           | 推荐方式                                           |
| -------------- | -------------------------------------------------- |
| 固定大小缓冲区 | 静态分配 /`static` 数组                          |
| 动态分配       | `malloc()` / `calloc()` → `free()`          |
| ISR 上下文     | 禁止使用动态分配                                   |
| RTOS 对象      | 优先`xTaskCreateStatic` / `xQueueCreateStatic` |

### 8.3 延时与阻塞

```c
// ✅ 使用 pdMS_TO_TICKS 转换时间
vTaskDelay(pdMS_TO_TICKS(100));

// ❌ 不要直接写 tick 数
vTaskDelay(100);  // 含义不明确
```

### 8.4 任务优先级分配

| 优先级范围                | 建议用途                   |
| ------------------------- | -------------------------- |
| 0 (idle)                  | 空闲任务                   |
| 1~3                       | 后台任务（日志、监控）     |
| 4~10                      | 业务逻辑任务               |
| 11~20                     | 实时敏感任务（传感器读取） |
| 21~configMAX_PRIORITIES-1 | 高优先级中断处理任务       |

---

## 9. 日志规范

### 9.1 日志宏

```c
// 每个源文件定义 TAG
static const char *TAG = "module_name";

// 日志级别从高到低
ESP_LOGE(TAG, "错误信息: %s", esp_err_to_name(ret));  // Error
ESP_LOGW(TAG, "警告信息");                            // Warning
ESP_LOGI(TAG, "普通信息: version %d", version);       // Info
ESP_LOGD(TAG, "调试信息: value=%d", value);           // Debug
ESP_LOGV(TAG, "详细信息");                            // Verbose
```

### 9.2 日志输出原则

- **ERROR**：不可恢复的错误，影响功能正常运行
- **WARNING**：可恢复的异常，不影响核心功能
- **INFO**：关键流程节点（启动、连接成功、升级完成）
- **DEBUG**：调试信息，生产环境应关闭
- **VERBOSE**：极详细的调试信息

### 9.3 格式化要求

```c
// ✅ 清晰的日志格式
ESP_LOGI(TAG, "[WiFi] 连接成功, SSID=%s, RSSI=%d dBm", ssid, rssi);

// ❌ 避免无意义的日志
ESP_LOGI(TAG, "ok");
```

---

## 10. SDK 配置管理

### 10.1 sdkconfig.defaults

该文件是项目的**默认配置模板**，必须提交到 Git：

```ini
# sdkconfig.defaults — 项目默认配置
CONFIG_IDF_TARGET_ESP32=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_FREERTOS_HZ=1000
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
```

### 10.2 多环境配置

```bash
# 开发调试配置
idf.py -DSDKCONFIG_DEFAULTS="configs/debug/sdkconfig.defaults" build

# 生产发布配置
idf.py -DSDKCONFIG_DEFAULTS="configs/release/sdkconfig.defaults" build
```

### 10.3 Kconfig 自定义选项

```kconfig
# components/my_component/Kconfig
menu "My Component Configuration"
    config MY_COMPONENT_ENABLED
        bool "Enable My Component"
        default y
        help
            Enable custom component functionality.

    config MY_COMPONENT_BUFFER_SIZE
        int "Buffer Size"
        default 256
        range 64 4096
        help
            Internal buffer size for data processing.
endmenu
```

### 10.4 硬件目标相关默认值

可以为不同芯片目标创建独立的默认配置文件：

```
项目根目录/
├── sdkconfig.defaults              # 通用默认值
├── sdkconfig.defaults.esp32        # ESP32 专用
└── sdkconfig.defaults.esp32s3      # ESP32-S3 专用（本项目）
```

构建时，IDF 会先加载 `sdkconfig.defaults`，再叠加对应目标的配置文件。

### 10.5 Kconfig.projbuild（顶层配置）

如需在 menuconfig 顶层（而非 `Component Configuration` 子菜单）添加配置选项，在组件目录创建 `Kconfig.projbuild` 文件。

### 10.6 bootloader_components（自定义引导加载程序）

```
项目/
├── bootloader_components/
│   └── main/
│       ├── CMakeLists.txt
│       ├── Kconfig
│       └── my_bootloader.c
```

可通过 `BOOTLOADER_IGNORE_EXTRA_COMPONENT` 变量按目标芯片条件选择引导加载程序。

---

## 11. CMake 代码风格

> ESP-IDF 构建系统使用 CMake。以下规范基于官方 v6.0.1 风格指南。

### 11.1 基本规则

| 规则 | 说明 |
|------|------|
| 缩进 | 4 个空格 |
| 行宽 | 最大 120 字符 |
| 命名（命令/函数/宏/局部变量） | 小写 + 下划线 (`with_underscores`) |
| 命名（全局变量） | 大写 + 下划线 (`WITH_UNDERSCORES`) |
| `endforeach()` / `endif()` | 不加参数 |

```cmake
# ✅ 正确
if(CONDITION)
    set(LOCAL_VAR "value")
endif()

# ❌ 错误 — 多余的括号参数
if(CONDITION)
    set(LOCAL_VAR "value")
endif(CONDITION)
```

### 11.2 组件注册

```cmake
idf_component_register(
    SRCS
        "src/my_component.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_wifi
        nvs_flash
    PRIV_REQUIRES
        esp_log
)
```

### 11.3 关键构建 API

| API | 用途 |
|-----|------|
| `idf_component_register()` | 注册组件源文件、依赖、include 路径 |
| `idf_build_set_property()` | 设置全局构建属性（编译选项等） |
| `idf_build_get_property()` | 读取全局构建属性 |
| `idf_build_process(target)` | 导入 ESP-IDF 组件并执行构建流程 |

---

## 12. CI/CD 与质量保障

### 12.1 GitHub Actions 构建示例

```yaml
# .github/workflows/build.yml
name: ESP-IDF Build

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [esp32, esp32s3]
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Build Firmware
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.3
          target: ${{ matrix.target }}
          path: '.'
```

### 12.2 提交前检查清单

| 检查项             | 要求                             |
| ------------------ | -------------------------------- |
| 编译               | `idf.py build` 无错误、无警告  |
| 代码格式           | 使用 `.clang-format` 统一格式化（官方使用 astyle，clang-format 为项目扩展） |
| pre-commit         | 安装并运行 pre-commit 钩子       |
| 单元测试           | 核心模块通过单元测试             |
| 硬件测试           | 在目标板上验证功能               |
| 文档               | 更新相关 API 文档和变更记录      |
| sdkconfig.defaults | 如有新配置项，同步更新           |

### 12.3 安装 pre-commit 钩子

```bash
cd $IDF_PATH
pip install pre-commit
pre-commit install

# 手动运行所有钩子
pre-commit run --all-files
```

> pre-commit 钩子会自动检查代码风格、行尾符、文件编码等。官方建议在首次克隆 ESP-IDF 后立即安装。

---

## 13. 附录：配置示例文件

### 13.1 `.editorconfig`

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{c,h}]
indent_style = space
indent_size = 4

[CMakeLists.txt]
indent_style = space
indent_size = 4

[*.md]
trim_trailing_whitespace = false
```

### 13.2 `.clang-format`

```yaml
BasedOnStyle: Google
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 120
AccessModifierOffset: -4
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
BreakBeforeBraces: Custom
BraceWrapping:
  AfterFunction: true
  AfterControlStatement: Never
PointerAlignment: Right
SortIncludes: true
IncludeCategories:
  - Regex: '^<.*\.h>'
    Priority: 1
  - Regex: '^"freertos/.*"'
    Priority: 2
  - Regex: '^"esp_.*"'
    Priority: 3
  - Regex: '^"driver/.*"'
    Priority: 4
  - Regex: '^".*"'
    Priority: 5
```

### 13.3 CMakeLists.txt 模板（顶层）

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 项目名称
project(box-demo)
```

### 13.4 CMakeLists.txt 模板（组件）

```cmake
idf_component_register(
    SRCS
        "src/my_component.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_wifi
        nvs_flash
    PRIV_REQUIRES
        esp_log
)
```

---

## 版本历史

| 版本 | 日期       | 变更说明                                                   |
| ---- | ---------- | ---------------------------------------------------------- |
| v1.0 | 2025-07-03 | 初始版本，涵盖仓库结构、代码风格、命名、错误处理等核心规范 |

---

> **维护者**：box-demo 开发团队
> **反馈**：如有规范相关疑问或改进建议，请提交 Issue 或 PR。
