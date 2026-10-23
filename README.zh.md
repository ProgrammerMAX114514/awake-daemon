# awake

![License](https://img.shields.io/badge/license-Apache--2.0-blue)
![Release](https://img.shields.io/github/v/tag/ProgrammerMAX114514/awake-daemon?sort=semver&label=release)
![CI](https://github.com/ProgrammerMAX114514/awake-daemon/actions/workflows/ci.yml/badge.svg)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2B-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)

[English](README.md) | [简体中文](README.zh.md)

> [!IMPORTANT]
> 本项目由 AI 生成，未经充分审查，使用前请谨慎甄别代码内容。作者不对使用本软件产生的任何后果负责。

当前版本：**0.6.0-beta**

`awake` 是一个 Windows 命令行工具，用于让系统保持唤醒。它由两个可执行文件组成：

- **awake.exe** - 客户端命令行工具。
- **awake.daemon.exe** - 后台守护进程，由客户端启动，不能直接调用。

## 下载

预编译包挂在[最新 Release](https://github.com/ProgrammerMAX114514/awake-daemon/releases/latest)下：

```
awake-daemon.win64.zip   awake.exe、awake.daemon.exe、LICENSE、NOTICE
```

每个发布 tag 都有 GPG 签名；Release 说明里列出了亮点、行为变更与完整命令表。解压到任意目录即可使用——两个可执行文件均静态链接，无需运行时或安装器。从源码构建见下文。

## 功能特性

- **保持唤醒**（`awake 1`）：只阻止*空闲*自动睡眠和休眠。电源按钮、开始菜单以及其他应用程序发起的手动睡眠指令不受影响，正常生效。
- **屏幕保持**（`awake screen on`）：在此之上让屏幕保持常亮——阻止屏幕关闭、屏幕保护程序以及因空闲超时触发的锁屏。
- **应用监视列表**：在 `awake.ini` 中配置的映像名；只要其中任何一个正在运行，守护进程就会以相同方式阻止空闲睡眠/休眠（每 30 秒检查一次）。
- **守护进程开机自启**（`awake daemon enable`）：可选地在用户登录时自动启动守护进程（写入当前用户注册表 Run 键，无需管理员权限）。
- 基于 ANSI 转义序列的**彩色控制台输出**。
- 需要时客户端会自动启动守护进程（`awake 0` / `awake 1`）。

## 环境要求

- Windows 10 或更高版本
- Visual Studio 2026（MSVC x64 工具集）
- CMake 4.x
- PowerShell 7（`pwsh`）——仅构建需要，运行工具不需要

工具链会自动发现；若 Windows SDK 不在默认位置，可设置环境变量 `AWAKE_SDK_ROOT`。

## 构建

在任意目录运行 `utils\build.bat`。该 bat 只是 `utils\build.ps1` 的启动器；PowerShell 脚本会准备 MSVC x64 环境（`utils\msvc-env.ps1`）并构建 Release 配置。它优先使用 Visual Studio 2026 的 CMake 生成器，无法使用时自动回退到 NMake 生成器，因此产物始终位于：

```
build\Release\awake.exe
build\Release\awake.daemon.exe
```

两个可执行文件均静态链接 C 运行时，运行时不依赖构建目录，可任意拷贝使用。

`utils\msvc-env.ps1` 优先调用 `vcvarsall.bat`；当它无法给出完整环境时（例如 `reg.exe` 被安全策略拦截），改为手工拼装 `INCLUDE`/`LIB`/`PATH`。

### 工具链配置（可选）

默认无需任何配置——所有路径都会自动探测。若要固定某个工具链，把 `utils\toolchain.ini.example` 复制为 `utils\toolchain.ini`（本地文件，不进 git）或 `%APPDATA%\awake\toolchain.ini`（所有 checkout 共用），按需取消注释：

```ini
[visual-studio]
path = C:\Program Files\Microsoft Visual Studio\18\Community

[windows-sdk]
root = C:\Program Files (x86)\Windows Kits\10

[cmake]
path = C:\Program Files\CMake\bin\cmake.exe

[generator]
name = Visual Studio 18 2026

[sevenzip]
path = C:\Program Files\7-Zip\7z.exe
```

优先级：环境变量 `AWAKE_TOOLCHAIN`（指向配置文件）> `utils\toolchain.ini` > `%APPDATA%\awake\toolchain.ini` > 自动探测。探测不到某个工具时，构建会询问一次路径并写回配置文件；非交互场景则直接报错并给出需要添加的配置项。全部可用条目见 `toolchain.ini.example` 内的注释。

## 测试

`utils/test/` 中提供了半自动化冒烟测试：

```
python utils\test\smoketest.py
```

它会遍历所有命令（包括别名和自启注册表项）并检查行为，由配置文件 `utils/test/smoketest.ini` 驱动（测试用应用、注册表位置、超时时间）。无法在普通 shell 中自动化的步骤——颜色效果、`powercfg /requests` 交叉验证、登录自启、手动睡眠行为——会在结尾以人工检查清单的形式列出。

单元测试位于 `tests/`（Catch2，已内置在 `tests/third_party/catch2`，无需联网）。它们与可执行文件一起构建，覆盖那些从命令行难以触达的部分——映像名校验、配置解析的行处理助手、以及包含「容忍未知字段」承诺的通信协议：

```
ctest --test-dir build --output-on-failure     # 或直接运行 build\...\awake_tests.exe
```

只想构建可执行文件时用 `-DAWAKE_BUILD_TESTS=OFF` 跳过。

## 持续集成

`.github/workflows/` 下有两个 GitHub Actions 工作流：

| 工作流 | 触发条件 | 内容 |
|---|---|---|
| `ci.yml` | 推送到 `main`、每个 PR、手动 | 用 `utils\build.bat` 构建 Release，校验二进制报告的版本与 `CMakeLists.txt` 一致，跑冒烟测试，并把可执行文件作为 artifact 上传。 |
| `release.yml` | 推送版本 tag（`0.*`）、手动指定 tag | 构建后**校验 tag 与构建出的版本一致，不一致拒绝发布**，打包 `awake-daemon.win64.zip` 与 `SHA256SUMS.txt`，用 `.github/release-notes/<tag>.md` 作为正文创建 GitHub Release。 |

因此发布流程变成：改 `project(VERSION ...)` → 提交 → 打 tag → 推送 tag。唯一值得手写的只有发布说明文件，缺失时工作流会退化为一段简短正文。

## 使用方法

```
awake help              显示帮助信息。
awake version           显示版本号。
awake 0                 取消保持唤醒（恢复系统默认电源行为）。
awake 1                 开启保持唤醒（阻止空闲睡眠和休眠）。
awake status            显示综合状态：守护进程、保持唤醒、屏幕保持和监视列表
                        （不会启动守护进程；监视列表条目绿色表示正在运行，
                        红色表示未运行）。
awake screen on         让屏幕保持常亮（阻止屏幕关闭、屏幕保护和空闲锁屏）。
awake screen off        停止让屏幕保持常亮。
awake daemon on         启动后台守护进程。
awake daemon off        停止后台守护进程。
awake daemon enable     开启守护进程开机自启（当前用户注册表 Run 键）。
awake daemon disable    关闭守护进程开机自启。
awake reload            让守护进程立即重新读取配置文件。
awake reset             重置配置文件（会要求二次确认）。
awake add <imagename>   向监视列表添加映像名。
awake del <imagename>   从监视列表删除映像名。
awake aliases           显示所有命令别名（也可写作 awake a / awake alias）。
```

命令别名：`awake ?` 和 `awake h` 等价于 `awake help`；`awake off`、`disable`、`stop` 等价于 `awake 0`；`awake on`、`enable`、`start` 等价于 `awake 1`；`awake st` 等价于 `awake status`；`awake scr` 等价于 `awake screen`；`awake screen 1` / `screen 0` 等价于 `screen on` / `screen off`；`awake d` 等价于 `awake daemon`；`awake daemon 1` / `daemon start` 等价于 `daemon on`；`awake daemon 0` / `daemon stop` 等价于 `daemon off`。

`awake aliases` 随时以下表形式输出同一份映射；帮助信息中它作为独立的「Command aliases」小节显示。

两个输出细节值得注意：

- 版本横幅会带上构建来源，例如 `awake 0.6.0-beta (git 3e548a6)`；带 `-dirty` 表示构建时有未提交改动，非 git 环境下只显示版本号。
- 每次交互式运行的末尾会在 **stderr** 输出一行「AI 生成」提醒。因此重定向或管道输出保持干净；可用 `AWAKE_AI_NOTICE=always` / `=never` 覆盖这一自动判断。

说明：

- `awake 0` 和 `awake 1` 在守护进程未运行时会自动启动它。
- 保持唤醒状态只保存在守护进程内存中，守护进程重启后回到未启用状态。
- 守护进程启动时会向系统发送一次性的"保持唤醒"通知（把空闲计时器清零一次），与当前设置无关。
- 首次运行会打印 Apache License 2.0 全文并询问是否同意；`help` 与 `version` 无需同意即可使用。同意结果写入 `awake.ini`，因此从旧版本升级后每个配置文件都会再询问一次。
- 在管理员终端运行客户端（或守护进程）时会打印一行黄色警告：在这种终端启动的守护进程无法被普通权限的 shell 控制。`awake status` 会用 `(elevated)` 标记提权的守护进程。

## 配置文件

`awake.ini` 与 `awake.exe` 位于同一目录（便携式）。文件不存在时会自动创建并写入说明注释。注释以 `#` 或 `;` 开头；在 `[watch]` 节内，其余每个非空行是一个映像名，例如：

```ini
[license]
license_accepted = true

[watch]
notepad.exe
```

`license_accepted` 记录首次运行时的协议同意状态：值为 `true` 以外、条目缺失、文件不存在或文件损坏时都会再次打印协议并询问。`[watch]` 以外的节存放 `key = value` 形式的设置，绝不会被当作监视条目。

当列表中的任何映像名正在运行时，守护进程会阻止空闲睡眠/休眠。推荐使用 `awake add` / `awake del` 编辑列表；修改会在 30 秒内自动生效，也可以用 `awake reload` 立即生效。如需清空所有条目并恢复初始模板，可运行 `awake reset`——执行前会警告并要求二次确认，覆盖文件后运行中的守护进程会立即重新加载配置（注意：这也会清除 `license_accepted`）。

## 项目结构

```
awake/
├── utils/
│   ├── build.bat      启动器：用 PowerShell 7 运行 build.ps1
│   ├── build.ps1      CMake 配置 + Release 构建（VS 生成器，失败回退 NMake）
│   ├── msvc-env.ps1   MSVC/SDK 环境准备（vcvarsall 或手工回退）
│   ├── toolchain.ps1  工具链配置读取、探测与交互式补录
│   ├── toolchain.ini.example  模板：可固定 VS/SDK/CMake/7z/Git 路径
│   ├── gen_license_text.py 从 LICENSE 重新生成 common/src/license.cpp
│   └── pubrelease.bat pubrelease.ps1 的启动器（打 tag + 构建 + 打包，需要 7-Zip）
├── CMakeLists.txt     构建定义（目标：awake、awake.daemon）
├── common/            客户端与守护进程共用代码
│   ├── include/       console（彩色输出）、config（awake.ini）、ipc（命名管道）、
│   │                  license（内嵌 Apache-2.0 全文）、privilege（提权检测）、
│   │                  process（进程枚举）、protocol（wire 协议）
│   └── src/
├── main/              客户端工具：awake.exe
└── daemon/            后台守护进程：awake.daemon.exe
```

## 工作原理

```
+-------------------+       named pipe           +----------------------------+
| awake.exe         |  \\.\pipe\awake_daemon     | awake.daemon.exe           |
| (客户端)          |  ------------------------> | (守护进程)                 |
|  main (分发)      |  基于行的 ASCII            |  main (启动/清理)          |
|  commands (命令)  |  请求/应答协议             |  server (管道服务循环)     |
|  status (状态)    |                            |  state (共享状态标志)      |
|  autostart (自启) |                            |  power (30 秒轮询线程)     |
+-------------------+                            +----------------------------+
                                                                 |
                                                                 v
                                                SetThreadExecutionState
                                                      Windows 电源管理器
```

- 守护进程通过 `SetThreadExecutionState` 持有 Windows 执行状态 `ES_CONTINUOUS | ES_SYSTEM_REQUIRED`，该状态只阻止空闲睡眠/休眠，不拦截手动睡眠请求。
- 一个专用的"电源线程"独占所有执行状态调用，按需并在至少每 30 秒一次的轮询中应用状态，退出前将其复位。
- 客户端与守护进程通过命名管道 `\\.\pipe\awake_daemon` 通信，使用简单的基于行的 ASCII 文本协议。互斥体保证守护进程单实例运行。
- 守护进程只有在被客户端以隐藏参数 `--internal-daemon <token>` 启动时才会运行，因此无法从命令行意外启动。

## 许可证

本项目基于 [Apache License 2.0](LICENSE) 授权，另见 [NOTICE](NOTICE)。

> **注意：** 本项目由 AI 生成，未经充分审查，使用前请谨慎甄别代码内容。作者不对使用本软件产生的任何后果负责。
