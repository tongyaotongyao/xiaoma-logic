# 资源清单

以下为原始资料位置、结构检查与实际 UE 导入结果。

| 资源 | 大小（约） | glTF 结构 | 用途 |
| --- | ---: | --- | --- |
| Character_Blue.glb | 76.4 MB | 1 mesh、1 material、3 images、0 skin、0 animation；约 86 万顶点 | 蓝色角色候选 |
| Character_Brown.glb | 75.9 MB | 1 mesh、1 material、3 images、0 skin、0 animation；约 85 万顶点 | 棕色角色候选 |
| Character_Pink.glb | 76.5 MB | 1 mesh、1 material、3 images、0 skin、0 animation；约 86 万顶点 | 粉色角色候选 |
| Character_Red.glb | 61.5 MB | 1 mesh、1 material、3 images、0 skin、0 animation；约 59 万顶点 | 红色角色候选 |
| Character_Yellow.glb | 75.3 MB | 1 mesh、1 material、3 images、0 skin、0 animation；约 85 万顶点 | 黄色角色候选 |

五个模型位于原始资料目录的 `3d-content/character-3d/`。Blender 渲染确认它们是卡通女孩，并非小马。它们没有骨骼和动画，且顶点数较高；“低模”只是原始命名线索，不能作为性能事实。原始模型不修改。

已使用 Blender 4.0 批处理制作六个项目内女孩 GLB 副本，位于 `Content/ImportSource/Characters/`。每个副本约 3 万三角面，贴图由 4096×4096 缩至 1024×1024；第六个 `Mint.glb` 基于蓝色模型制作薄荷色材质变体。蓝色副本从约 150 万三角面、76.4 MB 降至 3 万三角面、4.3 MB。已在 Blender 中渲染检查，并经 UE 5.5 Python 命令行导入 `Content/Characters/<颜色>/`，每色独立贴图与材质。

为了让棋盘符合网页原型，另用 Blender 制作了六色原创低模小马，位于 `Content/ImportSource/Ponies/`。每匹约 1884 三角面，没有骨骼或动画。已在 Blender 中渲染检查，并经 UE 5.5 导入 `Content/Ponies/`。棋盘使用这些小马；五个原始女孩模型已放置在主关卡周边作为装饰角色。主关卡 `Content/Maps/Main.umap` 含 45 个环境、角色和灯光 Actor，已由 UE 5.5 重新加载验证；正式 C++ 工程仍待编译运行。

原始网页使用 CSS 绘制小马；未找到可直接导入的音效、UI 图标或 3D 棋盘。`FreeParfaitSet` 是旧策划案的食物资产，本项目不使用。
