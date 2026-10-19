# 屏幕立体导航箭头

游戏 HUD 上方增加金色立体箭头，按实际镜头朝向显示下一段路线的方向。

- 指向下一个道路拐点；偏离路线时先引导回到道路。
- 显示取餐、送餐、返回驿站或找车，以及总剩余距离。
- 室内按三维位置选择当前路段，区分重叠楼层，提示上下楼。
- 到达时显示交互提示；无目标不绘制，无可达路线不显示方向。
- 手机、地图和暂停菜单打开时隐藏，返回游戏时恢复。

箭头由原生 Slate 投影有厚度的三维顶点并绘制面光照，不受场景亮度影响，无需额外 SceneCapture。旋转采用最短角度平滑插值。

## 验证

2026-10-19：Editor 编译成功，`CityCourier` 全部 100 项自动化测试通过（0 警告、0 失败）。新增测试覆盖左右拐弯、镜头朝向、偏离道路、楼层、上下楼、重复路径点和无效数据。

- 自动化报告：`Saved/Validation/Automation-20261019-182652-586d3213/Report/index.json`
- 真实 HUD 截图报告：`Saved/NavigationArrow/20261019T182753-76c4ad08/results.json`
- 重现截图：`scripts/Run-EditorScript.ps1 -Script Content/Python/capture_navigation_arrow.py -Gui`

下图来自真实 PIE 和原生 HUD，包含编辑器窗口边框；转动镜头验证使用实际 PlayerController。

![镜头朝向为 0 度时的导航](navigation-arrow/camera-forward.png)
![镜头右转 90 度后目标路线在身后](navigation-arrow/camera-right.png)
