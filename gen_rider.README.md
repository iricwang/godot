# gen_rider.bat — Godot 4.7 + JetBrains Rider 一键接入

把 Godot 4.7 引擎源码一键接入 JetBrains Rider：生成 `compile_commands.json`、
VS 项目文件、Rider 的 External Tools 和 Run/Debug 配置。

## 它干了什么

按顺序跑 5 步：

1. 初始化 MSVC 环境（`vcvars64.bat`，用 `cmd /D /C` 隔离避免宿主污染）
2. 检测 SCons（`scons` 或 fallback 到 `python -m SCons`）
3. 跑一次 `scons vsproj=yes vsproj_gen_only=yes compiledb=yes` —— 只生成项目文件，**不实际编译**
4. 写 Rider 配置：
   - `.idea/.idea.godot/.idea/workspace.xml`（2 个 Run/Debug 配置：`godot.editor.dev` 默认、`godot.editor.release`）
   - `.idea/.idea.godot/.idea/tools/External Tools.xml`（4 个 External Tool：Build/Clean × Debug/Release）
5. 写 `.idea/.idea.godot/.idea/.gitignore`（防止 Rider 临时文件污染 git）

## 使用方法

**从 cmd（不是 PowerShell）调起**：

```cmd
cd D:\AI_Temp\Godot\godot-4.7-beta
gen_rider.bat
```

跑完后：

1. Rider → File → Open → `D:\AI_Temp\Godot\godot-4.7-beta\godot.vcxproj`
   - 不要打开 `.sln`！项目入口是 `godot.vcxproj`，Rider 项目结构在 `.idea/.idea.godot/` 下的 xml 是按 vcxproj 绑定的
2. 等索引完成（首次 1-2 分钟）
3. 顶部右侧下拉选 run configuration，按 F5 启动
4. 顶部菜单 Tools → External Tools → Build Debug / Build Release / Clean Debug / Clean Release

## 编译参数说明

External Tools 的 4 个 tool 对应：

| Tool | 用途 | 产物 |
|---|---|---|
| **Build Debug** | 调试用（dev_build, debug_symbols, optimize=debug）| `bin\godot.windows.editor.dev.x86_64.exe` |
| **Build Release** | 性能用（默认 release 优化）| `bin\godot.windows.editor.x86_64.exe` |
| **Clean Debug** | 清掉 Debug 产物 | — |
| **Clean Release** | 清掉 Release 产物 | — |

Debug 版单步断点能跟、Release 版跑得快，按需切换。

## 重要说明 / 已知限制

- **不要从 PowerShell 调 `gen_rider.bat`**。PowerShell 宿主有时会污染 cmd 的 setlocal 状态，
  vcvars64 会报 `Microsoft was unexpected at this time.`。从 cmd 直接调就好。
- **不要重复编辑 `workspace.xml`**。每次跑 `gen_rider.bat` 都会重写这个文件（Rider 状态会丢）。
  如果你想保留个人 UI 状态（split 位置、最近文件等），把 `gen_rider.bat` 里的 `>` 改成 `>>`，
  然后手动合并。
- **`.scons_env.json` 不会被改**。scons 跑 `vsproj_gen_only=yes` 也会写这个文件，但只更新与 vsproj 生成相关的字段，不会改 `dev_build` / `optimize` 这些。
- **vcvars 路径硬编码**。脚本假定 VS2022 Community。如果你的不是，改 `gen_rider.bat` 里的 `VCVARS` 变量。
- **mono 模块是关的**。Rider 那边不需要 C#/Unity 集成。

## 故障排查

| 现象 | 原因 | 修法 |
|---|---|---|
| `Microsoft was unexpected at this time.` | 从 PowerShell 调 | 改用 cmd 调 |
| `vcvars64.bat not found` | VS 装在非默认路径 | 改脚本里的 `VCVARS` 变量 |
| `godot.sln was not generated` | scons 配置阶段出错 | 看 scons 输出，可能是依赖缺失 |
| Rider 打开后断点打不上 | 跑的是 Release 产物 | 改选 `godot.editor.dev` 或跑 `Build Debug` |
| 代码全红 | compile_commands.json 没生成 | 检查 Step 3 输出末尾 |
