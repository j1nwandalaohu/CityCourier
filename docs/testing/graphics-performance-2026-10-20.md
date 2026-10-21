# 1440p 高画质性能诊断

本次以保留画质为优先。保留原生场景分辨率、纹理、Lumen 全局光照、反射功能、阴影分辨率与距离；仅将高档 Lumen 反射的 `DownsampleFactor` 从 1 改为 2，由原有反射重建处理补齐结果。细小反射细节可能受影响。

## 对照结果

i5-13400F、RTX 3060 Ti 8GB、16GB RAM；真实 Windows Development 打包程序、D3D12、2560×1440、高档、100% 场景渲染分辨率、出生点静止视角、离屏渲染。每次采集 1500 帧，舍弃前 300 帧，统计后 1200 帧。

| 配置 | 平均 FPS | 1% low FPS | GPU 平均耗时 | 反射耗时 |
| --- | ---: | ---: | ---: | ---: |
| 原高档 | 38.74 | 31.10 | 25.09 ms | 3.00 ms |
| 仅反射采样调整 | 41.47 | 31.03 | 23.42 ms | 1.57 ms |

平均帧率提升约 7%；低帧没有改善，尚不能宣称卡顿已解决或达到稳定 60 FPS。这是短时静止诊断，不是三分钟道路/室内路线验收，也不是窗口实际呈现帧率。不同运行的温度和缓存存在波动。

原始 CSV：

- `Saved/Validation/Performance-NativeHigh-20261020-221734-7fc544cf/User/Saved/Profiling/CSV/`
- `Saved/Validation/Performance-NativeReflectionBudget-20261020-222228-ae793a39/User/Saved/Profiling/CSV/`

平均 FPS = 1000 / 平均 FrameTime；1% low = 1000 / 最慢 12 帧的平均 FrameTime。使用隔离的 `-UserDir` 和复制的配置、存档，未修改玩家进度。

## 采集注意事项

离屏运行必须使用 `-ForceRes -ResX=2560 -ResY=1440` 并核对日志中的 `systemresolution`；仅给 ResX/ResY 的早期采集实际降到了 888×500，不能作为 1440p 数据。

使用 `-ExecCmds` 中的 `csvprofile EXITONCOMPLETION,csvprofile FRAMES=1500` 在地图加载后开启 CSV；初始化阶段的 `-csvCaptureFrames` 在本机触发了 RHI 初始化断言。

缩短灯光距离、排除粗略阴影页中的非 Nanite 物体、TSR 全异步调度、减少软阴影采样均未产生可靠的额外收益，未纳入最终配置。自动降低分辨率和将高档阴影整体限制为中档的实验改动已撤回。
