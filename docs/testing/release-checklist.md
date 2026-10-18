# 首版发行检查（历史基线）

操作反馈、路线预览、独立导航和订单历史更新的构建、打包与验收见 [功能完善版验证](function-improvements-validation.md)。下表的 37 项测试和性能数字属于更新前首版，不作为本次重新测试结果。

首版交付检查通过。独立审查发现的1项必修问题已修复，最终37项自动回归通过；3项较小改进详见 [审查记录](final-review.md)。证据摘要保存在 [evidence/task12](evidence/task12)，原始日志、逐帧数据、截图和录音保存在 `Saved/Validation`。

| 项目 | 状态 / 证据 |
| --- | --- |
| UE5.6.1 编辑器编译 | 通过，Build-Editor-20261018-024533-1508e08a |
| UE 自动化回归 | 最终37/37，Automation-20261018-024544-ec25089b；13项PowerShell工具测试及性能统计/拒绝用例均通过 |
| 中档软件 Lumen | QualityPresets 红灯到绿灯，Automation-20261018-013514-042c1931 |
| 计算工具 | tests/Performance.Tests.ps1：均值、最慢1%、不达标拒绝、编辑器数据拒绝、非法帧拒绝通过 |
| Development 完整打包 | 最终包 Package-Development-20261018-024626-b9282ae8，退出0 |
| Shipping 完整打包 | 最终包 Package-Shipping-20261018-024722-a6c34abf，退出0；Task12-FinalShipping 实际键鼠接单、手机/地图/驿站Esc暂停、两次确认退出通过 |
| 打包版 8 单 / 4 商户 / 8 目的地 | Task12-PackagedRun/events.csv：8单、4商户、8不同目的地，全部准时送达；合计收入18200分，失败0 |
| 20 分钟稳定运行 | result.json：1331.447秒（22分11秒）、8867.60米；无崩溃、卡死或失败订单；真实骑行/步行/充电，没有路线传送、注入余额或修改计时 |
| 1080p 中档 3 分钟性能 | Task12-PackagedRun/frames.summary.json：180.0045秒、28446帧，平均158.03 FPS，1% low 98.25 FPS，通过 |
| 升级和收入重启保留 / 丢弃活动订单 | 正常收入购买动力/操控/电池各1级，余额3200分；result.json 与 reload.json 逐字段比较相同，活动订单从true变false，重启回站满电 |
| 中文 UI、音量、画质切换 | Task12-PackagedChecks：14张实际1080p截图，音量0.45/0.5、三档画质切换通过；保留实际主混音WAV |
| 保存失败、重试、救援、脱困、退出确认 | Task12-PackagedChecks/ui-result.json error为空；真实只读存档错误/恢复、两步救援、地图下方长按脱困、两步退出均通过。独立故障夹具使用位置传送，不作为连续路线证据 |
| 原创模型和声音 / 字体许可 | 资产清单、保留源文件、Noto OFL；318个已使用中文字符覆盖；两种完整包包含OFL/NOTICE及中文操作说明，OFL哈希与源文件一致 |
| 独立最终代码审查 | 完成：严重0、重要1已修复（Esc路径RED→GREEN），较小3项记录待改进；未安排重复审查 |

最终包对应运行时代码提交176e80d。22分钟连续配送和180秒性能采样对应9865be5；随后仅修复Esc菜单切换，不改变场景、移动、订单规则或渲染配置。修复后重新打包两种版本，并通过37项全套回归、Task12-FinalChecks（59.462秒）及Task12-FinalShipping实际键鼠验证，没有把旧包性能数据标为对新包的重新测量。
