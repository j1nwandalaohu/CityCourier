# 首版发行检查

独立打包、连续配送、性能和重启检查已通过；最后一次全套自动回归及独立代码审查正在收尾。证据摘要保存在 [evidence/task12](evidence/task12)，原始日志、逐帧数据、截图和录音保存在 `Saved/Validation`。

| 项目 | 状态 / 证据 |
| --- | --- |
| UE5.6.1 编辑器编译 | 通过，Build-Editor-20261018-023559-d5c7d375 |
| UE 自动化回归 | 36/36，Automation-20261018-015144-d7474290；最终检查脚本新增后待补跑 |
| 中档软件 Lumen | QualityPresets 红灯到绿灯，Automation-20261018-013514-042c1931 |
| 计算工具 | tests/Performance.Tests.ps1：均值、最慢1%、不达标拒绝、编辑器数据拒绝、非法帧拒绝通过 |
| Development 完整打包 | Package-Development-20261018-015705-05119429，退出0 |
| Shipping 完整打包 | Package-Shipping-20261018-015749-4a2d83e1，退出0；Task12-Shipping 实际键鼠打开手机、接单、步行、两次确认退出通过 |
| 打包版 8 单 / 4 商户 / 8 目的地 | Task12-PackagedRun/events.csv：8单、4商户、8不同目的地，全部准时送达；合计收入18200分，失败0 |
| 20 分钟稳定运行 | result.json：1331.447秒（22分11秒）、8867.60米；无崩溃、卡死或失败订单；真实骑行/步行/充电，没有路线传送、注入余额或修改计时 |
| 1080p 中档 3 分钟性能 | Task12-PackagedRun/frames.summary.json：180.0045秒、28446帧，平均158.03 FPS，1% low 98.25 FPS，通过 |
| 升级和收入重启保留 / 丢弃活动订单 | 正常收入购买动力/操控/电池各1级，余额3200分；result.json 与 reload.json 逐字段比较相同，活动订单从true变false，重启回站满电 |
| 中文 UI、音量、画质切换 | Task12-PackagedChecks：14张实际1080p截图，音量0.45/0.5、三档画质切换通过；保留实际主混音WAV |
| 保存失败、重试、救援、脱困、退出确认 | Task12-PackagedChecks/ui-result.json error为空；真实只读存档错误/恢复、两步救援、地图下方长按脱困、两步退出均通过。独立故障夹具使用位置传送，不作为连续路线证据 |
| 原创模型和声音 / 字体许可 | 资产清单、保留源文件、Noto OFL；318个已使用中文字符覆盖；两种完整包包含OFL/NOTICE及中文操作说明，OFL哈希与源文件一致 |
| 独立最终代码审查 | 待执行 |
