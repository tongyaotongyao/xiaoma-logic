# 小马逻辑

《小马逻辑》是用 UE 5.5.4 重建的 Windows 益智游戏，包含 25 个固定关卡和六色 3D 小马。每行、每列、每个颜色区域各放一匹，任意两匹小马不能在八个相邻方向上接触。

## 启动与操作

- 视觉更新版入口为 `Builds/Visual/Windows/XiaomaLogic.exe`。移动游戏时请一起复制整个 `Windows` 目录。基础版仍可从 `Builds/Win64/XiaomaLogic.exe` 启动。
- 编辑工程时用 UE 5.5.4 打开 `XiaomaLogic.uproject`，主场景为 `/Game/Maps/Main`。
- 左键放置或移除小马，右键标记或取消标记。也可打开标记模式后用左键标记。
- Z 撤销，R 重开，H 提示，Enter 提交，Esc 暂停。主菜单可切换音效。
- 游戏自动保存解锁进度、每关最佳星级、好感度、连胜、生命、音效设置和未完成的棋盘。

核心规则见 `GAMEPLAY_RULES.md`，原始资料见 `PROJECT_AUDIT.md` 与 `RESOURCE_INVENTORY.md`。环境和 MCP 见 `ENVIRONMENT_AUDIT.md`、`MCP_SETUP.md`，实测结果见 `TEST_REPORT.md`。

## 2026-11-19 空棋盘版本

最新版入口：`Builds/EmptyStart/Windows/XiaomaLogic.exe`。全部 25 关零预设、唯一解，重开清空棋盘。旧版进行中棋盘因关卡布局变化不再恢复；保留解锁、成绩和成长数据。旧的 Visual/Win64 构建不包含此修复。
