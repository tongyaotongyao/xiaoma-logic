# 环境审计

审计日期：2026-11-16（Asia/Shanghai）。

| 项目 | 检查结果 |
| --- | --- |
| 操作系统 | Windows 11 25H2，内部版本 26200.9974。注册表旧 `ProductName` 字段仍显示 `Windows 10 Home China`；UE 5.5.4 启动日志明确识别为 Windows 11 25H2。 |
| Codex | npm 安装的 Codex CLI 0.130.0；`codex.ps1` 位于 `C:\Users\20240\AppData\Roaming\npm`。桌面应用也在使用。 |
| Codex MCP 命令 | `codex mcp --help` 支持 `list/get/add/remove/login/logout`。 |
| Codex 配置兼容性 | 全局配置中 `service_tier = "default"` 与当前 CLI 不兼容；直接运行 `codex mcp list` 会报错。临时覆盖 `-c service_tier=fast` 可以正常读取配置。未改动原值。 |
| 已有 MCP | `blender`、`node_repl` 启用；`cua_repl` 禁用。未发现 Unreal MCP。 |
| Unreal Engine | UE 5.5，`D:\Epic Games\UE_5.5`。编辑器为 `Engine\Binaries\Win64\UnrealEditor.exe`，文件版本标识 `++UE5+Release-5.5-CL-40574608`。 |
| UE 构建 | `Engine\Build\BatchFiles\Build.bat` 存在。 |
| Visual Studio | Visual Studio Community 2026；Visual Studio 2022 Build Tools。`vswhere` 报告 VC 工具和 Windows 11 SDK 组件已安装。 |
| Node.js/npm | Node.js 24.14.1；npm 11.11.0。 |
| Python/Git | Python 3.14.3；Git for Windows 2.53.0。 |

## 构建修复

首次构建发现引擎缺失 `Engine/Intermediate/Build/BuildRules/UE5Rules.dll`。经用户授权，临时取消 installed-build 标记，运行 UnrealBuildTool 生成工程文件，成功重新生成该规则程序集，并立即恢复 installed-build 标记。

再次构建发现引擎仍缺少 `CoreNetTypes.generated.h` 和 `UnrealEditor-Core.lib`。通过 Epic Games Launcher 完成 UE 5.5.4 安装验证后，项目编辑器目标和 Win64 游戏目标均已构建成功。

UE 5.5.4 建议使用 MSVC 14.38，本机 14.51 有提示但已通过项目编译。MCP 的 Python 方案使用项目虚拟环境和现有 Python 3.12.11。配置及实测详见 `MCP_SETUP.md` 和 `TEST_REPORT.md`。
