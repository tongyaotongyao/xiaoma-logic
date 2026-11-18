# Unreal MCP 配置与验证

## 方案

本机运行 UE 5.5.4，安装中未发现 Epic 的官方 Unreal MCP 插件。[Epic Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)

最先安装了 `db-lyon/ue-mcp` 1.4.13。引擎修复后，项目 C++ 可以编译，该桥接插件仍在 Nanite、PoseSearch 和编辑器接口上出现 UE 5.5 编译错误。插件源码保留，工程描述文件中已停用。原服务曾返回 `editorConnected=false`。[db-lyon/ue-mcp](https://github.com/db-lyon/ue-mcp)

最终使用 ATDev UEMCP 0.1.0，MIT 许可，安装自审查过的提交 `3d38a7b2067275d9d0df7f932775204958a0e5a9`。服务兼容标准 MCP stdio，依赖 Python 3.10+ 和 MCP Python SDK。它使用 UE 自带的 Python 编辑器插件，省去自定义 C++ 桥接的编译迁移。与同样使用 Python 的 `sam-david/unreal-mcp` 相比，所选方案有独立许可证文件，并明确列出 UE 5.5 兼容范围。项目仍处于测试阶段，应以本机验证结果为准。[ATDev UEMCP](https://github.com/ATDev-Inc/uemcp) · [sam-david/unreal-mcp](https://github.com/sam-david/unreal-mcp)

## 实际配置

- Codex 注册名称为 `xiaoma_python`，通过 stdio 启动项目内 `Tools/uemcp-runtime/Scripts/uemcp.exe`，无需启动参数或指定工作目录。
- 环境变量为 `UEMCP_PROJECT=XiaomaLogic` 和 `UEMCP_MULTICAST_BIND=127.0.0.1`。UE 项目启用 Python 编辑器插件及远程执行，监听回环地址，组播 TTL 为 0。
- Python 远程执行可运行编辑器脚本，协议无身份认证。本配置用于本机，不放开防火墙和局域网监听。
- 打开本项目的编辑器后，可调用 `ue_status` 检查发现结果，随后用 `ue_project_info` 核对实际连接。关闭编辑器后服务仍可启动，但没有编辑器目标。
- 本机 CLI 如遇 `service_tier` 配置报错，可用 `codex -c service_tier=fast mcp get xiaoma_python` 检查注册项。

## 实际验证

2026-11-18，Codex 成功调用并发现 UE 5.5.4，读取 XiaomaLogic 工程、Main 关卡、场景 Actor 和资源列表。在独立关卡创建 StaticMeshActor，将其移动到 (120, 40, 150)，读回位置与组件，然后删除 Actor、返回 Main 并删除测试关卡。另以 `ue_python` 读取和修改项目静态网格的 Nanite 设置，实际保存资产。

已确认可用工具包括项目和场景读取、Actor 创建和位置修改、Actor 查询和删除、关卡创建和切换、资产查询和删除、编辑器 Python。服务还列出蓝图、材质、灯光属性、文件导入和运行视口工具，这些能力未分别完整验收。Widget Blueprint 与自动化测试没有专用工具，本项目用 C++ UMG、UnrealBuildTool 和 UE 命令行自动化完成。
