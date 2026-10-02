# awake

[English](README.md) | [简体中文](README.zh.md)

当前版本：**0.1.0-beta**

`awake` 是一个 Windows 命令行工具，用于让系统保持唤醒。它由两个可执行文件组成：

- **awake.exe** - 客户端命令行工具。
- **awake.daemon.exe** - 后台守护进程，由客户端启动，不能直接调用。

## 功能特性

- **保持唤醒**（`awake 1`）：只阻止*空闲*自动睡眠和休眠。电源按钮、开始菜单以及其他应用程序发起的手动睡眠指令不受影响，正常生效。
- **屏幕保持**（`awake screen on`）：在此之上让屏幕保持常亮——阻止屏幕关闭、屏幕保护程序以及因空闲超时触发的锁屏。
- **应用监视列表**：在 `awake.ini` 中配置的映像名；只要其中任何一个正在运行，守护进程就会以相同方式阻止空闲睡眠/休眠（每 30 秒检查一次）。
- 基于 ANSI 转义序列的**彩色控制台输出**。
- 需要时客户端会自动启动守护进程（`awake 0` / `awake 1`）。

## 环境要求

- Windows 10 或更高版本
- Visual Studio 2026（MSVC x64 工具集）
- CMake 4.x

工具链路径配置在 `build.bat` 中，请根据实际安装位置调整。

## 构建

在项目根目录运行 `build.bat`。它会初始化 MSVC x64 环境，使用 Visual Studio 2026 生成器配置 CMake 并构建 Release 配置。产物位于：

```
build\Release\awake.exe
build\Release\awake.daemon.exe
```

两个可执行文件均静态链接 C 运行时，运行时不依赖构建目录，可任意拷贝使用。

## 使用方法

```
awake help              显示帮助信息。
awake 0                 取消保持唤醒（恢复系统默认电源行为）。
awake 1                 开启保持唤醒（阻止空闲睡眠和休眠）。
awake status            显示当前保持唤醒状态（不会启动守护进程）。
awake screen on         让屏幕保持常亮（阻止屏幕关闭、屏幕保护和空闲锁屏）。
awake screen off        停止让屏幕保持常亮。
awake screen status     显示当前屏幕保持状态。
awake daemon on         启动后台守护进程。
awake daemon status     显示守护进程状态和受监视的应用程序
                        （绿色 = 在配置中且正在运行，红色 = 在配置中但未运行）。
awake daemon off        停止后台守护进程。
awake reload            让守护进程立即重新读取配置文件。
awake reset             重置配置文件（会要求二次确认）。
awake add <imagename>   向监视列表添加映像名。
awake del <imagename>   从监视列表删除映像名。
```

说明：

- `awake 0` 和 `awake 1` 在守护进程未运行时会自动启动它。
- 保持唤醒状态只保存在守护进程内存中，守护进程重启后回到未启用状态。
- 守护进程启动时会向系统发送一次性的"保持唤醒"通知（把空闲计时器清零一次），与当前设置无关。

## 配置文件

`awake.ini` 与 `awake.exe` 位于同一目录（便携式）。文件不存在时会自动创建并写入说明注释。注释以 `#` 或 `;` 开头；其余每个非空行是一个映像名，例如：

```ini
[watch]
notepad.exe
```

当列表中的任何映像名正在运行时，守护进程会阻止空闲睡眠/休眠。推荐使用 `awake add` / `awake del` 编辑列表；修改会在 30 秒内自动生效，也可以用 `awake reload` 立即生效。如需清空所有条目并恢复初始模板，可运行 `awake reset`——执行前会警告并要求二次确认，覆盖文件后运行中的守护进程会立即重新加载配置。

## 项目结构

```
awake/
├── build.bat          一键构建脚本（MSVC x64 + CMake）
├── CMakeLists.txt     构建定义（目标：awake、awake.daemon）
├── common/            客户端与守护进程共用代码
│   ├── include/       console（彩色输出）、config（awake.ini）、
│   │                  ipc（命名管道）、process（进程枚举）
│   └── src/
├── main/              客户端工具：awake.exe
└── daemon/            后台守护进程：awake.daemon.exe
```

## 工作原理

- 守护进程通过 `SetThreadExecutionState` 持有 Windows 执行状态 `ES_CONTINUOUS | ES_SYSTEM_REQUIRED`，该状态只阻止空闲睡眠/休眠，不拦截手动睡眠请求。
- 一个专用的"电源线程"独占所有执行状态调用，按需并在至少每 30 秒一次的轮询中应用状态，退出前将其复位。
- 客户端与守护进程通过命名管道 `\\.\pipe\awake_daemon` 通信，使用简单的基于行的 ASCII 文本协议。互斥体保证守护进程单实例运行。
- 守护进程只有在被客户端以隐藏参数 `--internal-daemon <token>` 启动时才会运行，因此无法从命令行意外启动。

## 许可证

本项目基于 [Apache License 2.0](LICENSE) 授权，另见 [NOTICE](NOTICE)。

> **注意：** 本项目由 AI 生成，未经充分审查，使用前请谨慎甄别代码内容。作者不对使用本软件产生的任何后果负责。
