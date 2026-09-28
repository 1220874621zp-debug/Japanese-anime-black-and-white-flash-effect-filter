<img width="2304" height="1728" alt="2" src="https://github.com/user-attachments/assets/c6ab68f8-996e-459f-a060-be667bfb66cc" />
<img width="1920" height="1080" alt="9月28日(1)" src="https://github.com/user-attachments/assets/b4713d39-6307-40dd-849b-6665db298b90" />
<img width="1920" height="1080" alt="9月28日" src="https://github.com/user-attachments/assets/16e7463d-b4c4-4ca7-9ba6-aa8f311a7dfb" />


# BWF 黑白闪 — 日式动画黑白闪光 AE 特效滤镜

原创 After Effects 特效插件（.aex），一键生成日式动画中经典的**黑白闪**冲击帧效果：受击、必杀、爆炸瞬间画面被撕成黑底白光（或自定义双色）的放射状闪光，带有从人物轮廓向外迸发的光线和速度感径向模糊。

> 本效果已同步移植到 Krita（见仓库简介），AE 版与 Krita 版算法一致。

## 效果原理（渲染管线）

```
输入图层
  → ① 阈值二值化（对比度拉伸 + 阈值切分，得到黑白两色图）
  → ② 符号距离场 SDF（边缘为 0，白色区域为正、黑色区域为负）
  → ③ 轮廓追踪 + Douglas-Peucker 简化（提取干净的主体轮廓线）
  → ④ 法线方向光线发射（以 SDF 梯度为法线，轮廓点沿法线向两侧发射光线，
       平方衰减 + 软边光斑，叠加成放射状光图）
  → ⑤ 径向模糊（以指定中心做放射状采样模糊，强化速度感）
  → ⑥ 着色输出（结果在「背景色 ↔ 闪光色」之间插值，再按闪光强度与原图混合）
```

全过程无状态、逐帧计算，可直接配合关键帧 / 表达式做出闪烁节奏。

## 参数说明

| 参数 | 默认值 | 说明 |
|---|---|---|
| 阈值 | 34 | 二值化亮度阈值（0–255） |
| 对比度 | 142% | 二值化前的对比度拉伸 |
| 边缘强度 | 50 | 轮廓光线的整体强度系数 |
| 光线/模糊中心 | 图层原点 | 法线发光与径向模糊的共用中心点 |
| 光线强度 | 80 | 轮廓迸发出的光线亮度 |
| 光线长度 | 196 | 光线从轮廓向外延伸的距离（像素） |
| 轮廓简化 | 1 | Douglas-Peucker 容差，越大轮廓越简、光线越利落 |
| 法线采样密度 | 1 | 轮廓点采样间隔，越大光线越稀疏 |
| 模糊强度 | 15 | 径向模糊强度（与到中心的距离成正比） |
| 模糊品质 | 15 | 径向模糊采样数（2–16，越高越平滑） |
| 闪光强度 | 100 | 效果与原图的混合比例，0 = 原图 |
| 闪光颜色 | 白 | 亮部颜色 |
| 背景颜色 | 黑 | 暗部颜色 |

## 安装

1. 从 [Releases](../../releases) 下载 `BWF.aex`（Windows 64 位）；
2. 复制到 AE 插件目录，例如
   `C:\Program Files\Adobe\Adobe After Effects <版本>\Support Files\Plug-ins\`；
3. 重启 AE，在 **效果 → Sample Plug-ins → BWF** 中调用。

## 编译

依赖：

- [After Effects SDK 25.6](https://console.adobe.io/plugins/aftereffects)（其它近版本 SDK 一般也可）
- CMake ≥ 3.20
- MinGW-w64 GCC（C++17）

```bash
# 指向本机 AE SDK 根目录（ae25.6_61.64bit.AfterEffectsSDK 那一层）
cmake -B build -DAE_SDK_ROOT="D:/path/to/ae25.6_61.64bit.AfterEffectsSDK"
cmake --build build
# 产物：build/BWF.aex
```

源文件为 UTF-8 编码（中文参数名），编译时已按 `-finput-charset=UTF-8 -fexec-charset=GBK` 转换，请勿改动源文件编码。

## 技术特性

- 8bpc 与 16bpc 双精度渲染（Deep Color Aware）
- 支持多线程渲染（SUPPORTS_THREADED_RENDERING）
- 纯 CPU 无外部依赖，静态链接运行库
- 版本 v5.0.0（AE SDK 25.6 / AEProc Intel x64）
