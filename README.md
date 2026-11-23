本项目为很早以前的promicro主控的nageki连接新的io（ https://github.com/Sanheiii/ongeki-io ）做了一个兼容层，老的io打不上mod而且适配的segatools也不知道是哪年的东西于是鞭策ai产出了这么个东西，以下为ai生成的readme



# MU3Input LegacyBridge

[English](docs/README.en.md) · [协议与架构](docs/architecture.md) · [测试与限制](docs/testing.md) · [发布清单](docs/releasing.md)

为旧款 Arduino Leonardo 音击控制器提供的 **Windows x64 / MU3Input 3.1 PC 端灯光兼容桥**。无需刷固件，也不覆盖原 IO：转发原输入/Aime API，将新版六灯 RGB 重新打包为旧固件的十槽布局，并使用独立的异步 HID writer。

这是一个范围有限的兼容项目，不是完整 IO、游戏启动器或通用 Arduino 驱动。与上游项目无隶属或官方支持关系。

## 适用范围

| 项目 | 要求 |
| --- | --- |
| 系统/进程 | Windows x64；非 x86、ARM64 或跨平台版本 |
| 原 IO | 自行取得的 MU3Input 3.1，具有所需 18 个函数出口 |
| USB 身份 | VID `2341` / PID `8036` / UsagePage `FFC0` / Usage `0C00` |
| HID 报告 | 无非零 ReportID，应用 payload 64B，Windows 输入/输出长度65B |
| 灯光 | board 1、RGB18，旧槽0–2与5–7，各三灯 |

VID/PID 不是设备唯一标识。其他设备即使身份相同也不保证兼容；多台匹配设备没有序列号选择。新固件/其他布局不应盲目使用。

## 安装

1. 完全退出宿主、后台、IOConfig 等 IO 程序；备份自己的配置。
2. 从本项目构建或可信发布包取得桥 DLL，**与原版 DLL 放在同目录**：

   ```text
   MU3Input.dll                 # 原版3.1，自行取得，不在本仓库提供
   MU3Input.LegacyBridge.dll     # 本项目；不要重命名成 MU3Input.dll
   MU3Input.LegacyBridge.ini     # 从 config 复制
   ```

3. 在自己的现有配置中只修改两个入口，其他选项保持原样：

   ```ini
   [mu3io]
   path=MU3Input.LegacyBridge.dll

   [aimeio]
   path=MU3Input.LegacyBridge.dll
   ```

4. 保留有效的原输入 JSON。MU3Input 读取**宿主当前工作目录**中的 `mu3input_config.json`；桥的 INI 则从桥自身目录读取。
5. 重启并分阶段检查输入、低亮度灯光、后台，再验证实际宿主。

桥以绝对路径加载同目录原 DLL，不从当前目录或 PATH 寻找替代。不要把旧时代只有 `mu3_io_set_led(uint)` 的 DLL 当作3.1替代。

### 灯光设置

```ini
[bridge]
enabled=1
brightness=40
```

- `enabled=0` 只关闭桥的 LED writer，输入/Aime 转发仍工作。
- `brightness` 是8位范围的数值，限制在0–40；**40不是40%**。
- 首次调用读取一次，修改后须退出所有相关进程再重启。

### 原 IO 配置注意事项

HID `Param` 必须是对象而非 `null`。以下是 `IO` 数组中的**一个条目**，不是完整配置模板：

```json
{
  "Type": "Hid",
  "Param": {
    "Vid": 9025,
    "Pid": 32822,
    "UsagePage": 65472,
    "Usage": 3072
  },
  "Scope": "All"
}
```

不要覆盖已有服务键/其他后端设置。如果只使用 USB HID，可移除不需要的 `Udp` 后端；两个原IO进程绑定相同 UDP 端口可能导致未捕获异常。`Scope=None` 不阻止后端构造或端口绑定；删除整个/损坏 JSON 可能回落默认 UDP。桥不修复这些上游配置行为。

## 构建与测试

需要 Windows x64、PowerShell、MinGW-w64 GCC（以 GCC 14.2.0 本地验证）。例如在 MSYS2 MINGW64 环境安装 `mingw-w64-x86_64-gcc`，然后在 PowerShell 中把其 `bin` 加入 PATH：

```powershell
./build.ps1
./test.ps1
```

也可以显式传编译器：`./build.ps1 -Gcc 'D:/tools/mingw64/bin/gcc.exe'`。

输出位于 `build/`。构建含 `-Wall -Wextra -Werror`。离线测试只加载本项目的 fake DLL 并注入模拟 I/O，**不加载原版 MU3Input，不枚举/读写真实控制器，不启动宿主**。GitHub Actions 配置执行同类构建和测试；配置存在不等于远端 CI 已运行。

发布包生成方法见[发布清单](docs/releasing.md)。源仓库不应提交 `build/`、`dist/`、原 DLL、固件或现场日志。

## 安全边界和已知限制

- 拦截 board 1 的颜色回调并立即复制18字节，不保留宿主栈指针，不转发原版灯光颜色函数。桥只生成 Type 0 灯光命令，没有 Type 1 卡号写入路径；**原 IO 的其他功能并未因此变为只读**。
- 输入/Aime 保持原版行为。原版输入接收、异常处理、取消寿命或重连缺陷不在本桥修复范围内。
- 多进程通过同会话的命名互斥量串行写；两个常驻进程要求不同颜色仍可能交替覆盖。没有游戏优先级，不排斥其他软件或其他 Windows 会话。
- 最高约29.4Hz，只有最新帧，可能丢弃中间动画帧。同色空闲500ms后释放设备并让权1秒，重开后重发同色。
- 超时取消后等待真正完成，避免复用内核仍使用的数据。驱动永不完成取消时，worker可能永久停住并保留资源；进程退出是最终恢复边界。
- 桥固定模块到进程结束，**不支持运行时卸载/热更新**。
- 未签名的本地构建不是杀毒软件信誉保证。不要为运行它盲目关闭安全软件，也不要求一律管理员运行。

## 验证状态

在一台匹配设备上，六灯低亮度顺序/颜色/最后熄灭已由操作者确认；桥接配置下后台约119秒未出现新增应用错误，输入探针12秒584个样本保持连接。公开仓库仅保留汇总结论，不附个人日志或游戏数据。

这些不是完整游戏、长期压力、多设备兼容或实际热插拔的保证。详见[测试矩阵](docs/testing.md)。

## 回滚

退出所有相关进程，再将两个 `path` 恢复成自己的备份值；保留原 DLL 即可。恢复原路径也会恢复原 LED writer 的问题。若只需临时隔离灯光，优先使用桥 INI 的 `enabled=0` 后重启，不要恢复无关的旧 UDP 配置。

## 许可与来源

本桥代码采用 [MIT](LICENSE)。原版 IO、固件及其他程序不包含在本仓库，也不受本项目 MIT 许可重新授权。协议/ABI参考与第三方边界见 [NOTICE](NOTICE.md)。仅在你有权使用的设备和软件环境中使用。
