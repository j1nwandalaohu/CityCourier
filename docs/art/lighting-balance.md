# 主地图室内外亮度调整

街道在夕阳阴影下过暗，室内小房间的白墙和家具则大面积过曝。主地图后期体积固定曝光，不能通过整体增加曝光同时解决两端的问题。

调整范围：

- `L_CourierCity` 的天空补光由 0.55 提高到 1.65，保留太阳角度、直射光和固定游戏曝光。
- 原生室内矩形灯由统一 1800 lm 改为商店 240 lm、住宅 180 lm、办公室 300 lm；保留灯具位置、色温、阴影、照射范围和按距离加载。
- `integrate_environment_art.py` 同步新的天空强度，避免以后重新导入美术时还原亮度。

只更新天空参数时运行 `apply_lighting_balance.py`，室内输出需编译 C++。不必重建全城几何或室内数据资产。

`capture_lighting_balance.py` 在正式地图 PIE 中使用游戏曝光、五个固定镜头和原生灯具截图；商店、住宅、办公室均检查真实组件的灯光输出。角色临时移到镜头附近以触发正常详情驻留，截图完成后结束 PIE，不保存测试角色或相机。此脚本没有摄影曝光、额外灯光或运行时光照覆盖。

原始对比在 `Saved/Validation/LightingBalance/comparison/before-*.png`，最终保存版本复查在 `Saved/Validation/LightingBalance/final/`。对比中的 `preview-*` 是中间调参试验，最终室内灯光比该试验进一步降低。

此次不更改渲染质量或进行性能优化；此前代表路线的 1% low 未达到目标，不能将亮度调整视为性能验收通过。

本次编辑器编译通过；最终五个机位截图完成且无脚本错误，原生灯具实测分别为 240、180、300 lm；`CityCourier.Buildings.Art.CookedCoverageAndDoorContinuity` 回归通过。截图证据使用保存后的地图与新编译的灯具代码。
