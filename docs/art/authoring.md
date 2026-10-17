# 原创资产制作与复现

角色、电动车、楼房、店面、摊位、街道设施和招牌均由本项目制作，没有使用 GTA 或第三方模型。字体使用 Noto Sans CJK SC，许可与版权声明保留在 `AssetsSource/CityCourier/Fonts`。

源场景：`AssetsSource/CityCourier/CityCourier_Original.blend`。打开后显示骑手和骨架；其他资产保留独立名称，可在 Outliner 逐个显示。四组动作保留在 Action 列表中。源尺寸为厘米，X 向前、Z 向上。FBX 转换会翻转 Y 轴，门面和招牌的场景朝向由布置脚本处理。

本机 Blender 4.0.2 位于 `D:\sdk\Blender_4.0\Blender Foundation\Blender 4.0\blender.exe`，来自已有且数字签名验证有效的 Blender Foundation 安装包，使用管理解包方式部署。

按顺序复现，执行时关闭占用同一项目资产的编辑器、游戏和自动化测试：

```powershell
& 'D:\sdk\Blender_4.0\Blender Foundation\Blender 4.0\blender.exe' --background --python tools/art/build_courier_assets.py
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_art.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/dress_city.py -EngineRoot D:/sdk/UE_5.6
uv run --with fonttools python tools/art/validate_font.py
./scripts/Test.ps1 -Filter CityCourier.ArtAssets -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier.Scooter -EngineRoot D:/sdk/UE_5.6
```

`asset-manifest.csv` 记录稳定导入路径和来源。导入脚本使用源文件 SHA-256 判断是否需要重导入，显式保存骨架、材质和动画依赖。动画蓝图使用四个 Sequence Player，按实际速度和骑乘状态选择，包含姿态混合。车辆的 RiderSeat、LeftGrip、RightGrip、LeftPedal、RightPedal 组件是骑姿定位依据。

布置脚本只更新自己管理的 `CC_` 标签，未标记的手工场景对象保持不变。街道设施为道路与两条近路保留净空。关键资产必须通过实机截图与实际碰撞检查；资产可加载并不等于美术验收通过。

背景轿车和普通服装行人的源文件为 `AssetsSource/CityCourier/Ambient/CityCourier_Ambient.blend`。运行 `tools/art/build_ambient_assets.py`（Blender）生成这两项 FBX 和七项 WAV，再用 `Run-EditorScript.ps1 -Script Content/Python/import_ambient.py -Gui` 导入。音频导入使用完整编辑器以初始化 Bink 解码器；命令行 Python commandlet 缺少该解码器。

七项声音全部由项目脚本合成：低通噪声与低频谐波构成城市底噪，120/360/720Hz 谐波构成电机循环，短衰减噪声构成制动声，四组不同音高序列区分接单、取餐、送达和失败。源文件为 44.1kHz、单声道、16bit PCM；随机种子固定，无采样外部录音。声音源和模型均进入资产清单，未使用其他游戏音源。
