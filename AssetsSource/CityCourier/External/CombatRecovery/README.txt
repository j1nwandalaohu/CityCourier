免费战斗与起身素材组合（2026-10-22）

状态：Content Examples 前后中等受击动作已导出、重定向并导入主角资源。主角与 NPC 共 34 个动画资源已替换；未执行原生构建、动画图编译、自动化测试或启动游戏。

一、优先复用下午已下载的 Content Examples

LocalSamples/ContentExamples/Content 保留原始目录结构与字节完全相同的来源快照：
- 3 个拳击资源：MontageExample_Punch_1、MontageExample_Punch_2、Punch_Montage。
  包含动画片段与蒙太奇，不代表 3 个独立拳法。
- 15 个 MM_HitReact_* 受击资源，含方向、强度及 additive/montage 变体。
- 8 个 MM_Death_* 倒地候选资源，含同动作的根运动变体。
  已检查导出动作的末帧：并非完整躺倒，未用作最终击倒。主角与 NPC 的倒地/起身改用本地 Mesh2Motion 的 Death_D / LayToIdle。

这些快照不是完整依赖迁移；骨架等依赖仍在原始项目：
D:/UnrealProjects/ContentExamples/Content
已从原项目导出所选前后受击，转换后导入骑手骨架。不要把此快照目录直接复制到运行时 Content。
拳击使用已有 Mesh2Motion CC0 动作；这些 Content Examples 拳击候选仍仅作参考。

二、新增下载的 CMU 地面起身

Downloads/Direct/140.asf：配套 Acclaim 骨架。
Downloads/Direct/140_01.amc：Get Up Face Down，趴地起身，830 帧，120 FPS，约 6.91 秒。
Downloads/Direct/140_08.amc：Get Up From Ground Laying on Back，仰卧起身，898 帧，120 FPS，约 7.48 秒。

这三个交付文件均通过直连下载（curl --noproxy *），保留原始数据。
检查了连续帧编号、每帧 29 个通道、各通道自由度和有限数值；哈希在 manifest.json。
ASF/AMC 不能直接当作 UE AnimSequence 使用；后续需转换为 FBX/BVH、处理单位与根运动、重定向、裁剪并检查姿态衔接。
帧结构检查不是动作视觉质量验收，也不代表原始录制没有抖动或穿插。

来源与许可：
CMU https://mocap.cs.cmu.edu/search.php?subjectnumber=140
原始文件 http://mocap.cs.cmu.edu/subjects/140/
FAQ http://mocap.cs.cmu.edu/faqs.php
CMU 原始数据许可允许复制、修改和再分发，可用于商业产品，但不得直接转售数据本身；不是 CC0。
来源声明：The data used in this project was obtained from mocap.cs.cmu.edu.
The database was created with funding from NSF EIA-0196217.
Epic 的 Content Examples 素材遵循获取时 Epic/Fab 示例内容条款，不是 CC0。
Downloads 中保留来源页面与许可页面；manifest.json 与 docs/art/asset-manifest.csv 记录文件、来源和哈希。

三、当前完成与待办

- 主角和 NPC 已使用素材库拳击；NPC 原手写手臂轨迹已移除，素材伸拳峰值对齐既有命中时间。
- 倒地/起身使用完整仰卧动作组合；CMU 原始数据保留作参考，没有声称已接入。
- 主角与 NPC 的旧动作路径已直接更新；资源来源和保存哈希见 docs/art/all-character-animation-replacements.json。
- 静止待机分支与新增主角连续受击倒地逻辑仍需本轮之外授权的 Editor 构建和动画图生成；未运行游戏验收。
