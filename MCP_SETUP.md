# Unreal MCP 配置与验证

## 方案选择

本机 UE 为 5.5。Epic 官方 Unreal MCP 文档对应较新的 UE 版本；本机 UE 5.5 安装中没有该官方插件，因此不能直接使用官方方案。

选用 `db-lyon/ue-mcp` 1.4.13，MIT 许可。其项目说明明确支持 Windows 上 UE 5.4–5.9、Node.js 20+ 和 Codex。仓库在审计时活跃，提供项目级 C++ 桥接插件。代价是需编译插件，并允许本机 MCP 服务操作打开的 UE 工程；只安装在本项目中，不装到引擎目录。安装包 SHA-256：`1003304006F9A951E49997E2296B8BDEEC7DA3C78DDB39DC2D84B0017D4E3C5F`。

项目来源：<https://github.com/db-lyon/ue-mcp>。Epic 官方方案说明：<https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor>。

## 当前状态

- 固定版本包已安装在项目的 `Tools/mcp-runtime`，插件源码已安装在 `Plugins/UE_MCP_Bridge`。
- `XiaomaLogic.uproject` 已启用桥接插件。Codex 用户级配置已加入 `xiaoma_ue`，项目级配置也存在；实际启动命令使用本地 Node 与固定版本包，不依赖每次运行时从 npm 下载。
- `codex -c service_tier=fast mcp get xiaoma_ue` 确认配置可读取。本次 Codex 会话已经能调用 MCP 服务端的 `project.get_status`；返回 `mode=disconnected`、`editorConnected=false`、`pluginBuildStale=true`。这只证明服务端可访问，不表示编辑器连接成功。
- 插件与项目编译因 UE 5.5 安装缺少 `CoreNetTypes.generated.h` 和 `UnrealEditor-Core.lib` 而阻塞。规则程序集 `UE5Rules.dll` 已经重建。
- Codex CLI 当前需 `-c service_tier=fast` 才能读取现有全局配置；未改动原 `service_tier`。原配置副本已保存在 `Codex-system/Codex-setup/config.before-xiaoma-mcp.2026-11-16.toml`。

## 待验证能力

编辑器状态、工程与关卡、工具列表、Actor 读写和清理、蓝图、UMG、导入、材质、场景灯光、构建与自动化测试。
