# 素材存储规则

本项目按单人、单机开发管理。GitHub 保存代码和少量关键玩法资源；大型美术文件保留在本机原路径。不制作素材包，不维护素材版本号、素材锁文件或全量快照。

| 内容 | 保存位置 |
| --- | --- |
| Source、Config、项目文件、脚本和工具 | 普通 Git |
| 设计说明、精简报告、来源、许可证、下载链接、导入参数及已有源文件校验记录 | 普通 Git |
| Content/CityCourier/Blueprints 下的直接子级 uasset | Git LFS |
| Content/CityCourier/Input 下的直接子级 uasset | Git LFS |
| Content/CityCourier/Maps 下的直接子级 umap | Git LFS |
| Content/CityCourier/Data/DA_CityDefinition.uasset | Git LFS |
| Content/CityCourier/Animation/Epic 下的直接子级 uasset，包括动画图、选择器、数据库、重定向及配置 | Git LFS |
| 模型、贴图、动画片段、音效、字体等美术资源 | 本地原路径 |
| AssetsSource 中的 Blender、FBX、GLB、贴图、下载包及其他二进制文件 | 本地原路径 |
| 原始 GASP、Stack O Bot、Content Examples 和实验对比资源 | 本地原路径 |
| 大批截图和视频 | 本地原路径；仓库仅保留现有 README 示例图 |
| Saved、Intermediate、DerivedDataCache、Binaries、Artifacts | 本地，可按用途重新生成 |

`.gitignore` 是跟踪范围的执行规则；`.gitattributes` 将允许跟踪的 uasset 和 umap 放入 LFS。不要用 `git add -f` 把忽略的美术文件加入仓库。新增玩法资源应先扩充明确的允许名单；不要把整个 Content 或 AssetsSource 放开。

## 日常开发

直接编辑当前工程中的素材，不需要打包或发布素材版本。提交时包括相关代码、必要玩法资源、生成脚本及来源说明。执行 `python tools/repository/check_tracking_policy.py` 可检查暂存区是否混入应仅在本地保存的文件，以及玩法资源是否存为 LFS 指针；这不是 UE 构建或游戏测试。

独创或修改过且难以重做的 Blender 等源文件，在重大修改前按需单独备份到另一块盘或其他备份位置。可重新下载或可靠生成的素材不重复备份。忽略文件仍保留在本机；不要使用 `git clean -fdx`，它会删除被 Git 忽略的本地素材。

代码回退不会自动回退本地美术。换电脑时复制实际需要的 `Content` 美术文件和编辑源文件；仅克隆仓库不能直接运行或重新生成全部画面。蓝图和地图也可能引用未随 Git 传输的本地美术，这是当前方案接受的取舍。


## 远程分支清理

本提交只清理当前分支文件列表，保留全部父提交与代码历史。被移出的素材仍保留在原开发机；不会操作其工作目录。历史中的大文件仍然存在，仓库历史体积不会因这一提交立即缩小。
