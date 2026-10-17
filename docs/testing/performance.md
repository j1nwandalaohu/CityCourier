# 打包性能实测

2026-10-18 实测通过：平均 **158.03 FPS**，**1% low 98.25 FPS**。采样来自关闭编辑器后的 Windows Development 独立包，运行目录为 `Saved/Validation/Task12-PackagedRun`；打包日志 `Package-Development-20261018-015705-05119429`。

实测机器：Intel Core i5-13400F，NVIDIA RTX 3060 Ti 8GB，16194 MB 系统内存，D3D12。`metadata.json` 由游戏进程读取实际硬件、窗口和画质控制台值生成。

| 指标 | 实测 |
| --- | ---: |
| 稳定段时长 / 帧数 | 180.0045 秒 / 28446 帧 |
| 平均 FPS | 158.03 |
| 1% low FPS | 98.25 |
| 游戏线程平均耗时 | 2.77 ms |
| 渲染线程平均耗时 | 6.31 ms |
| GPU 平均耗时 | 5.45 ms |
| 进程物理内存峰值 | 1425.55 MB |
| RHI 纹理分配峰值 | 773.68 MB |

前 30 秒预热独立保留 4619 帧，平均 153.95 FPS，最慢帧 380.08 ms，第一帧 116.84 ms；这些加载/初始化帧不混入稳定段。不把已经安装和运行过的机器描述为首次安装或冷缓存测试。原始逐帧数据为 `frames.csv`，计算结果为 `frames.summary.json`；不是编辑器、无渲染模式或理论估算。

测量配置：1920×1080 窗口，中档软件 Lumen GI 和反射，硬件光追关闭；83% 内部分辨率重建至 1080p。关闭垂直同步和帧率上限。预热 30 秒，随后单独标记 180 秒固定道路路线，其余配送和充电帧不计入该窗口。

固定路线按 `(12000,-12000) → (12000,0) → (12000,12000) → (0,12000) → (-12000,12000) → (-12000,0) → (-12000,-12000) → (0,-12000)` 厘米循环。控制器使用正常车辆移动、转向和刹车接口，保留背景人车、碰撞和电量消耗；遇车会减速绕行，不传送车辆或修改配送计时。

`frames.csv` 来自实际帧间单调时钟；游戏线程和渲染线程时间来自 UE RenderTimer，GPU 时间来自 RHI 时间戳查询。进程物理内存来自 FPlatformMemory，纹理显存来自 RHI 的 streaming + non-streaming texture allocations，后者不代表全部显存占用。GPU 查询返回零时标记为不可用，不把它解释为 GPU 无耗时。

```powershell
./scripts/Measure-Performance.ps1 -CsvPath <打包运行输出目录>/frames.csv
```

脚本只接受 metadata 和样本均标记为 PackagedGame 的 1080p 中档配置，拒绝编辑器数据、非正/非有限帧时和不足 179 秒的采样段。平均 FPS = 1000 / 平均帧时；1% low = 1000 / 最慢 1% 帧时的平均值。通过目标为平均 ≥30 FPS 且 1% low ≥24 FPS。

开发版验收入口为 `-CourierValidation`；`-CourierValidationMode=Checks` 使用独立故障场景检查 UI，`-CourierValidationMode=Reload` 检查重启进度。该驱动不会在 Shipping 中启动。性能结果必须来自已关闭编辑器且没有同时进行编译、烘焙或第二个 UE 实例的运行。

完整稳定性运行持续 1331.447 秒、211403 帧，骑行 8867.60 米，完成 8 单并购买三项升级，没有崩溃或订单失败。全程进程物理内存峰值 1654.73 MB，RHI 纹理分配峰值 773.68 MB；`nvidia-smi` 每秒采样的整块 GPU 显存使用峰值为 3332 MiB，包含桌面和其他应用，不能表述为本游戏专用显存。摘要见 `evidence/task12/stability-metrics.json`。
