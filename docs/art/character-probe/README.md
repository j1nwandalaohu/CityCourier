# 人物动作与写实度试验（2026-10-18）

在独立美术 worktree `city-courier-art` / `feat/city-courier-art` 完成两项试验。Mesh2Motion 动作已重定向到现有 MakeHuman 骑手，并在 UE 实际播放。CharMorph 已做独立同镜头、同灯光近景；本轮效果尚不值得替换正式角色，也没有达到照片级。

合入 main 后的兼容验证见 [合并记录](../../testing/merge-validation.md)。本页截图与视频保留美术分支的原始试验版本。

## 实际画面

动作画面均为 **左：原动画；右：Mesh2Motion 重定向**。两边使用同一个正式骑手网格、服装和骨架。

| 项目 | 预览 |
| --- | --- |
| 慢跑，世界速度 4.2 m/s | [对比视频](jog.mp4) · [定格](jog-comparison.png) |
| 快跑，世界速度 7 m/s | [对比视频](run.mp4) · [定格](run-comparison.png) |
| 其他动作定格 | [待机](idle-comparison.png) · [步行](walk-comparison.png) |
| 中性灯光人物近景 | [MakeHuman](makehuman-neutral.png) · [CharMorph](charmorph-neutral.png) |
| 暖色主光人物近景 | [MakeHuman](makehuman-dusk.png) · [CharMorph](charmorph-dusk.png) |

视频由实际 UE PIE 截帧编码，1280×720、15 fps、各 2 秒，附左右标注；用于观察姿态、步幅及世界空间脚底滑动，不代表运行帧率。Epic 画质、固定曝光、三盏面积灯和同一环境光；一米网格固定于世界空间，镜头和灯光随移动角色跟随。人物近景使用相同镜头和曝光，无景深遮掩。暖光图是测试棚中的 4400K 主光，并非正式街区实景。

## Mesh2Motion 重定向

离线使用 Mesh2Motion 仓库附带的 CC0 动作，在 Blender 中映射到现有 50 根骨骼，再输出 FBX 导入 UE。没有向在线服务上传角色，也没有重新自动绑定骑手。

| 上游动作 | 试验资产 | 时长 | 估算自然速度 | 对应游戏速度 / 播放倍率 |
| --- | --- | --- | --- | --- |
| Idle_A | A_M2M_Idle | 3.133 s | 待机 | 0 / 1 |
| Walk | A_M2M_Walk | 1.667 s | 0.799 m/s | 用于低速步行试验 |
| Jog | A_M2M_Jog | 1.167 s | 4.732 m/s | 4.2 m/s / 0.887629 |
| Sprint | A_M2M_Run | 0.833 s | 7.159 m/s | 7 m/s / 0.977842 |

骨架名称、层级及参考矩阵保留。核心骨使用动作旋转增量，四肢校正静止轴向，双腿用两骨 IK 匹配脚踝轨迹；手掌按中指根部的实际方向对齐，并映射三十根手指关节。统一 30 fps，根骨静止，循环终帧闭合，鞋底作每段固定高度校正。

Blender 全采样帧中，新动作根骨位移为 0，最低鞋底约 +2 mm。根据离地不超过最低点 2.5 cm 的鞋底顶点组估算接触阶段，匹配游戏速度后的慢跑/快跑脚底滑动中位数约 **3.93 / 4.43 cm/s**。这是网格采样代理指标；动态接触、插值帧及完整 UE 蒙皮接地仍需进一步验收。[逐段数值](../../../AssetsSource/CityCourier/motion_probe_validation.json)

画面中摆臂、握拳、抬腿和躯干前倾比原动作完整。仍有肩颈服装穿插、手臂权重与循环衔接的不足；慢跑脚部在循环边界存在速度变化。本页保留原始候选动作对比；新版慢跑、快跑现已提升至正式角色，见 [正式接入记录](../production-actions.md)。

## CharMorph 写实度

使用 CharMorph-Vitruvian 的 Default + Male + EastAsian 形体，统一全身高度 1.76 m，导出着衣静态半身。保留原始 EXR，派生 4K 头部与 2K 其他贴图，按原 UV 烘焙，接入颜色、粗糙度、切线法线与次表面材质。半身约 13.8 万顶点、25.9 万三角面，尚未做 LOD。

为便于判断，两边去除头盔，以相同 UE 灯光、镜头和 Subsurface 材质类别比较。两者并非同一个人，也不是同一件衣服；这里比较完整导入结果，不能单独归因于拓扑或纹理分辨率。

本轮 CharMorph 仍显得皮肤平淡、眼睛缺少湿润层次，眉毛偏硬；增加面数与纹理分辨率没有直接获得照片感。这个结论适用于当前转换方案：源 Blender 程序材质经简化，使用原浅色皮肤和高度派生法线，未保留完整角膜折射与泪线，未制作头发和睫毛，也未做专门的 UE 皮肤着色、表情或动画绑定。后续应优先完善这些部分，再判断是否替换人物来源。[导入材质及限制](../../../AssetsSource/CityCourier/character_comparison_materials.json)

## 免费资源与许可

本轮外部模型、纹理及动作采用 CC0，采购费用 0。原始文件、来源清单、版本和 SHA-256 随仓库保留。

CharMorph 上游 `.gitattributes` 的原始字节另存为 `license-evidence/upstream.gitattributes.txt`，避免其 LFS 过滤规则将本仓库中的原始模型重新转为指针；下载清单保留上游文件名及哈希。

| 来源 | 固定版本 | 许可证据 |
| --- | --- | --- |
| [Mesh2Motion 动作资源](https://github.com/Mesh2Motion/mesh2motion-app/tree/653a9698f5f315523072f1c3ff3496100401317b/static/animations) | 653a9698f5f315523072f1c3ff3496100401317b | 上游 README 将模型/绑定/动画列为 CC0；保留 LICENSE-CC0.MD。应用程序代码的 MIT 与这些资产许可分别记录。 |
| [CharMorph-Vitruvian](https://github.com/Upliner/CharMorph-Vitruvian/tree/c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44) | c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44 | 上游 config.yaml 明确 `license: CC0`；保留配置、作者信息和 CC0 法律文本。未打包 GPL 的 CharMorph 插件代码。 |

## 验证与隔离

- 116 个原始来源文件 SHA-256 校验通过；CharMorph 另在下载时核对 Git blob / LFS 哈希。
- Blender 全帧骨架一致性、根骨及鞋底采样检查通过。
- UE 最终录制 68 张，无脚本错误；64 个动作采样中，运行角色与压缩动画的五个检查骨位置最大误差约 **0.000071 cm**，根骨漂移为 0。[录制结果](capture-results.json)
- 新进程 UE 全套测试 **26/26 通过**：`Automation-20261018-182551-b0d51459/Report/index.json`。[保留摘要](verification.json)
- 最终导入关卡：`L_CharacterProbe`；生成日志 `EditorScript-20261018-182154-f9a214f2`，录制日志 `EditorScript-20261018-182251-99274521`。日志原件在 `Saved/Validation`。
- 已复核正式骑手、共享骨架、四段原动画、正式预览地图、Config 和 C++ 均未改变；另一 Codex 的玩法 worktree 未修改。通用导入脚本排除两个试验导出目录，避免误当普通静态资源导入。

试验目录为 `/Game/CityCourier/Art/Experiments/{Mesh2Motion,CharacterComparison}`，关卡为 `/Game/CityCourier/Maps/L_CharacterProbe`。两个源工程位于 `AssetsSource/CityCourier/Blender/{Mesh2MotionProbe,CharMorphProbe}.blend`。

## 重现

使用仓库配置的 Blender 4.5.3 和 UE 5.6。从本美术 worktree 执行；Blender 命令中的 `blender` 指本机安装路径。

```powershell
python tools/art/fetch_motion_probe.py
python tools/art/fetch_charmorph_probe.py
blender -b --python tools/art/build_motion_probe.py
blender -b --python tools/art/validate_motion_probe.py
blender -b --python tools/art/build_makehuman_bust.py
blender -b --python tools/art/build_charmorph_probe.py
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_motion_probe.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_character_probe.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_character_probe.py -EngineRoot D:/sdk/UE_5.6 -Gui
blender -b --python tools/art/encode_character_probe.py
python tools/art/verify_sources.py
./scripts/Test.ps1 -Headless -EngineRoot D:/sdk/UE_5.6
```

正式骑乘、动作契约中其余十段动作、最终发行性能及照片级验收尚需独立推进。
