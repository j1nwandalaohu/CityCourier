# 人物动作功能验证（2026-10-18）

功能工作树：`D:/eleme/.worktrees/city-courier`，分支 `feat/city-courier`。用户确认美术工作树刚开始制作新增动画；本次交付功能代码与接入接口，不将缺少的动画列为已完成的视觉效果。

## 已实现范围

- 步行空格真实起跳，沿用骑车空格刹车；低顶、空中重复起跳、动作锁与落地前 0.12 秒限制。
- 左右键按下沿输入，同帧优先重拳；轻拳左右衔接、接重拳、三击上限、窗口外输入拒绝。
- 每拳唯一实例、有效时间区间、跨帧区间采样、拳部骨骼轨迹、墙体遮挡、130／160 cm 范围和每目标去重。
- 前后受击接口及不叠加硬直；攻击限速 120 cm/s，受击、离地、救援、结算和界面切换打断。
- 暂停保留动作进度，非暂停界面清理攻击；空中打开手机继续自然下落。
- `FullBodyAction` 动作槽、`CurrentAction`／`ActionElapsed`／`bAirborne`、左右拳挂点，以及跳跃、落地和跑步起停的稳定资源路径。
- 可放置的原生 `CourierTrainingTarget` 测试靶，显示命中次数和回弹，不改街区内路人行为。

新增动作资产未到位时，出拳被明确拒绝并提示；不会用待机／走路动画冒充出拳，也不生成不可见命中。没有新增生命值、死亡、体力、NPC 反击或经济惩罚。

## 自动化证据

基线 `Automation-20261018-173121-c90090a9` 为 51/51。新增测试涵盖时间边界、整段有效帧跨越、连击替换／上限、动作取消、真实跳跃冲量、头顶空间、空中输入、暂停、落地门禁、命中遮挡、范围、去重、前后受击、缺资源释放，以及实际 Montage 槽参与姿态。

最终功能桥接测试在 `Automation-20261018-175756-47263eea` 为 60/60；加入 Development 实机验证驱动后 `Automation-20261018-180009-2511d16a` 仍为 60/60。报告逐项检查 Success，失败／未执行均为 0。

独立审查后最终全套为 `Automation-20261018-181636-c304d497`：**63/63**，另覆盖命中回调中取消／打开界面／移除攻击者、缺 AnimInstance／缺动作槽／前摇中停止 Montage，以及薄墙初始重叠的不同目标数量。完整审查与处理见 [审查记录](character-actions-review.md)。薄墙场景在原代码已通过，没有把它算成失败复现。

RED 记录保留在 `Saved/Validation/actions-task1-red.log`、`actions-task1-green2.log`（World 尚未实现）、`actions-task3-red.log`、`actions-edges-red.log`、`actions-bridge-red.log`。额外回归复现并修正：整拳跨帧时的朝向丢失、初始重叠命中点缺失，以及挂点未标记包脏导致新进程读不到。

## 夹具与真实运行的区别

`CharacterActionContactTests` 和部分 World 测试在独立角色实例的 `ActionAnimations` 中借用已有 `A_Walk`，验证实际骨骼采样、碰撞、动作状态与 Montage 槽。夹具不会保存到角色资产或包中；这些结果不证明轻重拳姿态已经完成。

`Actions-Editor-01` 使用实际 `-game` 运行、正常 CharacterMovement 与场景碰撞，10.136 秒通过：起跳并实际升高，开手机时保留竖直速度、重力继续，正常落地，缺动画拒绝出拳，暂停／返回隔离正确。`actions.json` 明确记录缺少 10 段动画，`new_animation_visual_acceptance=false`。首次截图与界面更新同帧，后续已把截图与手机打开／HUD 刷新分开，初次图片不作为动作视觉证据。

所有实机验证使用独立 `-UserDir`，不读写玩家真实存档。没有重测长时间性能或音频试听。

## 打包与回归

最终包对应运行时代码 `ae990f1`，Development 与 Shipping 均完整成功，ExitCode=0；SHA256 见 [packages.json](evidence/character-actions/packages.json)。Shipping 完整目录 34 个文件、704412184 字节，约 672 MiB。

| 检查 | 实际结果（日志位于 `Saved/Validation`） |
| --- | --- |
| Development | `Package-Development-20261018-181724-367c18d0` |
| Shipping | `Package-Shipping-20261018-181838-664587cb` |
| 最终打包动作运行 | `Actions-Packaged-Final`，10.010 秒，`error` 为空；真实起跳、空中手机重力、落地、缺动画拒绝、暂停及界面隔离通过；明确报告缺少 10 段动画 |
| 最终真实配送 | `Actions-Route-Final`，70.116 秒、256.83 米，预览后接单、绕行驿站／充电、找车、取餐、骑行送达；完成 1／失败 0、余额 1600 分，无传送、注入收入或加速计时 |
| 配送重启 | `Actions-Route-FinalReload`，11.007 秒，余额 1600 分、完成 1／失败 0、单条历史一致，无活动订单 |
| Shipping 实际键盘 | `Actions-Shipping-Final`，实际按住空格，截图观察到离地与随后落地，没有长按连续跳跃；定向 Tab／Esc 事件观察到手机与暂停界面；测试进程正常关闭 |

实际鼠标检查遭遇桌面焦点干扰，途中停止全局鼠标模拟；本轮**不宣称 Shipping 物理左右键检查通过**。碰撞、连击、同帧输入聚合及界面隔离已由实际组件／原生动画测试覆盖，左右键实际体验仍应在美术动作到位、桌面焦点稳定后联合检查。定向窗口消息结果不冒充物理鼠标验收。

最终打包截图已目视检查：角色真实离地、落地，底部跳跃／轻重拳提示可见，“出拳动画尚未就绪，请等待美术资源接入”明确可读。截图不证明尚未制作的新跳跃／拳击姿态。所有本轮测试进程均已退出，未终止美术侧的运行。

JSON、报告及上述有效截图保存在 [evidence/character-actions](evidence/character-actions)。首次 `180139/180307` 包、初次 `Actions-Route-01` 和 `Actions-Editor-01` 保留作诊断；当前交付以本表最终包为准。本轮未更改资料格式；完整 63 项仍包含已有迁移、历史、存储与配送回归。

## 美术交接

按 [动作约定第 7 节](../art/character-actions-contract.md) 导入后，编译功能侧 Editor，再运行 `Content/Python/build_character_actions.py` 重建动作槽和输入，重启运行。共享骨架挂点的默认偏移为 `(5,0,0)` cm；重建保留美术调整后的偏移。新增十段的时长、原地动作和视觉衔接仍需双方联调验收。
