# ESP32-S3 + ESP-IDF v6.0.1 环境搭建完全指南

> 适用场景：Windows 11、中文用户名（如 `卢冠宇`）、多 Python 版本共存、代理环境  
> 目标芯片：ESP32-S3（8MB Flash + 8MB Octal PSRAM）  
> 本项目：`box-demo` — 交互式图形仪表盘（LovyanGFX + ST7789）

---

## 1. 问题诊断

### 1.1 EIM GUI 卡死

**现象：** 启动 `eim-gui-windows-x64.exe` 后界面长时间无响应

**根因：**
- EIM 启动时执行 4 轮镜像测速（GitHub、Espressif CDN、PyPI 等共 ~9 个 URL）
- 所有流量走代理 `http://127.0.0.1:7890/`（Clash/V2Ray）
- `mirrors.aliyun.com` 返回 403、部分连接 TLS 握手超时 → 测速阶段挂死

**结论：** 放弃 EIM GUI，改用纯命令行安装（更可靠、更快）

### 1.2 Python 3 未找到（`python3` 在 cmd.exe 下不可用）

**现象：** EIM 日志报 `No working Python 3 found on Windows PATH`

**根因：**
- PowerShell 有 `python3` 的 App Execution Alias（`WindowsApps\python3.exe`）
- 但 **cmd.exe 不识别**这个别名
- EIM（Rust/Tauri 原生应用）调用 `CreateProcess("python3", ...)` 走的是 cmd 风格查找 → 找不到

**验证方法：**
```powershell
python3 --version         # PowerShell OK
cmd /c "python3 --version"  # cmd.exe  FAIL
```

**解决：** 在 Python 安装目录下复制 `python.exe` → `python3.exe`（见 2.2 节）

### 1.3 中文路径编码损坏（`pyvenv.cfg` 乱码）

**现象：**
```
# pyvenv.cfg 中的乱码：
home = C:\Users\鍗栧啝瀹嘰AppData\Local\Programs\Python\Python313
#          ^^^^^^^^^^ 应为 卢冠宇
```

**根因：** 用户名 `卢冠宇` 的 UTF-8 字节 `E5 8D A2 E5 86 A0 E5 AE 87` 被某种工具以错误编码写入配置文件，显示为 `鍗栧啝瀹嘰`（13 字节，非法的 GBK->UTF-8 双重编码）

**影响：** venv 的 `home` 指向不存在的乱码路径 -> Python 虚拟环境异常

> 此问题同样出现在 `PATH` 环境变量、`eim_idf.json` 等文件中

---

## 2. Python 环境修复

### 2.1 确认 Python 安装

```powershell
# 直接调用完整路径验证
& 'C:\Users\卢冠宇\AppData\Local\Programs\Python\Python313\python.exe' --version
# -> Python 3.13.5
```

### 2.2 创建 `python3.exe`（必经步骤）

```powershell
Copy-Item `
  'C:\Users\卢冠宇\AppData\Local\Programs\Python\Python313\python.exe' `
  'C:\Users\卢冠宇\AppData\Local\Programs\Python\Python313\python3.exe'

# 验证 cmd.exe 下可用
cmd /c "python3 --version"
# -> Python 3.13.5
```

### 2.3 确保 Python 在系统 PATH 中

```powershell
# 检查用户 PATH
[Environment]::GetEnvironmentVariable('PATH', 'User') -split ';' | Select-String python

# 应包含：
#   C:\Users\卢冠宇\AppData\Local\Programs\Python\Python313\
#   C:\Users\卢冠宇\AppData\Local\Programs\Python\Python313\Scripts\

# 如果缺失，手动添加（需要管理员权限修改系统 PATH）
```

### 2.4 修复 `pyvenv.cfg` 乱码

```powershell
$cfgPath = 'C:\Espressif\tools\python\v6.0.1\venv\pyvenv.cfg'
$content = Get-Content $cfgPath -Raw
$fixed = $content -replace '鍗栧啝瀹嘰', '卢冠宇'
Set-Content -Path $cfgPath -Value $fixed -Encoding UTF8
```

> 后续改用 `install.ps1` 重新创建 venv 时不会再出现此问题，因为 Python 3.13 的 venv 模块正确支持 UTF-8

---

## 3. ESP-IDF v6.0.1 命令行安装（三步法）

### Step 1：克隆源码（含全部子模块）

```powershell
New-Item -ItemType Directory -Force -Path "C:\ESP-IDF\.espressif\v6.0.1"
cd C:\ESP-IDF\.espressif\v6.0.1
git clone -b v6.0.1 --recurse-submodules --depth 1 `
  https://github.com/espressif/esp-idf.git esp-idf
```

> `--recurse-submodules` 是关键：确保 21 个子模块一次性初始化，避免后续 cmake 报 `submodule out of date`
>
> 如果代理导致 TLS 错误，加 `--jobs 4` 降低并发：
> ```powershell
> git clone ... --recurse-submodules --depth 1 --jobs 4 ...
> ```

### Step 2：运行 install.ps1

```powershell
$env:IDF_PATH = 'C:\ESP-IDF\.espressif\v6.0.1\esp-idf'
$env:IDF_TOOLS_PATH = 'C:\Espressif\tools'
cd $env:IDF_PATH
.\install.ps1 esp32s3
```

`install.ps1` 做的事：
- 下载 11 个工具链到 `C:\Espressif\tools\tools\`（xtensa-elf-gcc、cmake、ninja、openocd…）
- 创建 Python 3.13 venv 到 `C:\Espressif\tools\python_env\idf6.0_py3.13_env\`
- 安装 58 个 Python 包（esptool 5.3.0、idf-component-manager 3.0.3…）

> 如果 tools 已存在，install.ps1 会跳过下载（但会重新解压到 `tools\tools\` 子目录）

### Step 3：验证

```powershell
. "$env:IDF_PATH\export.ps1"
idf.py --version
# -> ESP-IDF v6.0.1
```

### 最终目录结构

```
C:\ESP-IDF\.espressif\v6.0.1\esp-idf\   <- 源码 + 全部子模块（~2GB）
C:\Espressif\tools\tools\                <- 工具链（cmake、ninja、gcc…）
C:\Espressif\tools\python_env\           <- Python venv + pip 包
C:\Espressif\tools\dist\                 <- 工具压缩包缓存
C:\Espressif\tools\idf-env.json          <- install.ps1 生成的安装记录
```

---

## 4. VS Code 配置

### 4.1 工作区设置（`.vscode/settings.json`）

```json
{
  "idf.espIdfPath": "C:\\ESP-IDF\\.espressif\\v6.0.1\\esp-idf",
  "idf.toolsPath": "C:\\Espressif\\tools",
  "idf.pythonInstallPath": "C:\\Espressif\\tools\\python_env\\idf6.0_py3.13_env\\Scripts\\python.exe",
  "idf.flashType": "UART"
}
```

| 配置项 | 说明 |
|--------|------|
| `idf.espIdfPath` | ESP-IDF 框架源码路径 |
| `idf.toolsPath` | 工具链根目录（含 `idf-env.json`） |
| `idf.pythonInstallPath` | **install.ps1 创建的 venv** 中的 python.exe |

> 不要指向 EIM 创建的旧 venv（`tools\python\v6.0.1\venv\`）

---

## 5. sdkconfig 修正清单

构建前必须手动修正 `sdkconfig` 中以下 5 项（直接用编辑器修改）：

### 5.1 分区表 -> 自定义 CSV

```
# CONFIG_PARTITION_TABLE_SINGLE_APP is not set
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

项目的 `partitions.csv` 包含 SPIFFS `storage` 分区，默认分区表不含它。

### 5.2 Flash 大小 -> 8MB

```
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="8MB"
```

### 5.3 PSRAM -> 启用 Octal 模式

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
```

> 缺少 OCT 模式会导致 `E (175) quad_psram: PSRAM chip is not connected` 然后 `abort()`！

### 5.4 FreeRTOS 时钟 -> 1000Hz

```
CONFIG_FREERTOS_HZ=1000
```

项目所有 `vTaskDelay(pdMS_TO_TICKS(10))` 依赖 1ms tick。

### 5.5 一键检查

```powershell
Select-String -Path sdkconfig -Pattern 'PARTITION_TABLE_CUSTOM|FLASHSIZE_8MB|SPIRAM_MODE_OCT|FREERTOS_HZ'
# 应全部显示 "=y" 或 "=1000"
```

---

## 6. 构建与烧录

### 6.1 完整构建

```powershell
# 激活 ESP-IDF 环境
. "C:\ESP-IDF\.espressif\v6.0.1\esp-idf\export.ps1"
cd d:\snowsong\box-demo

# 清理并构建
Remove-Item build -Recurse -Force -ErrorAction SilentlyContinue
idf.py build
```

### 6.2 烧录

```powershell
idf.py -p COM9 flash        # 烧录
idf.py -p COM9 flash monitor  # 烧录 + 串口监听
```

烧录分区布局：
| 分区 | 地址 | 内容 |
|------|------|------|
| bootloader | 0x00000000 | 引导程序 |
| partition-table | 0x00008000 | 分区表 |
| factory (app) | 0x00010000 | box-demo 主程序 |
| storage (SPIFFS) | 0x00210000 | 图片/音频资源 |

### 6.3 仅监听串口（不重新烧录）

```powershell
idf.py -p COM9 monitor
# 退出：Ctrl+]
```

---

## 7. 常见错误速查

| 错误信息 | 原因 | 修复 |
|----------|------|------|
| `No working Python 3 found on Windows PATH` | cmd.exe 下 `python3` 不存在 | 创建 `python3.exe`（见 2.2） |
| `Failed to resolve component 'LovyanGFX'` | 子模块未克隆 | `git clone --depth 1 https://github.com/lovyan03/LovyanGFX.git components/LovyanGFX` |
| `Failed to create SPIFFS image for partition 'storage'` | 分区表不是自定义 CSV | 设置 `CONFIG_PARTITION_TABLE_CUSTOM=y`（见 5.1） |
| `Partitions occupies 6.1MB ... does not fit in configured flash size 2MB` | Flash 大小默认 2MB | 设置 `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y`（见 5.2） |
| `PSRAM chip is not connected, or wrong PSRAM line mode` | 缺 Octal 模式 | 设置 `CONFIG_SPIRAM_MODE_OCT=y`（见 5.3） |
| `submodule ... is out of date` | 浅克隆子模块版本不匹配 | 用 `--recurse-submodules` 重新克隆 |
| git TLS/SSL 错误（代理环境） | 代理中断 HTTPS | 用 `--jobs 4` 降低并发，或临时关闭代理 |
| GitHub push 403 | HTTPS 密码认证已禁用 | 使用 Personal Access Token 或 SSH 密钥 |

---

## 8. 已废弃的清理项

```powershell
# 删除 EIM 创建的旧 ESP-IDF
Remove-Item "D:\EE\ESP-IDF" -Recurse -Force

# 删除 EIM 创建的旧 venv（路径含乱码）
Remove-Item "C:\Espressif\tools\python" -Recurse -Force

# 删除 EIM GUI 自身（如果不再使用）
# Remove-Item "C:\Users\卢冠宇\.espressif\eim_gui" -Recurse -Force
```

---

> 本文档基于 2026-06-25 在 `snowsong42/box-demo` 项目的实际故障排查和修复过程整理。
