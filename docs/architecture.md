# 协议、ABI与线程边界

## 数据路径

```text
host -> bridge -> original MU3Input 3.1 -> original input/Aime backends
             \-> RGB18 copy -> latest frame -> worker -> legacy HID Type 0
```

17个原函数直接转发，`mu3_io_led_set_colors` 被拦截。`mu3_io_led_init` 仍转发原版。另导出 `bridge_encode_led`（纯编码）和 `bridge_shutdown`（停止writer），共20个函数出口；不导出原版 NativeAOT 调试数据。

## x64 ABI

显式使用固定宽度类型，禁止把 Win32 `BOOL` 当作1字节的原版布尔值。完整签名位于源码包的 `src/bridge.c`，出口位于 `src/bridge.def`（二进制包不含源码）。关键边界：

- HRESULT形返回：`uint32_t`；API版本：`uint16_t`。
- `mu3_io_is_busy` / `mu3_io_is_connected` 返回 `uint8_t`，后者参数为 `int32_t`。
- 摇杆输出 `int16_t *`，按钮为 `uint8_t *`。
- Aime ID缓冲长度为 `uint64_t`；Felica ID/PM为 `uint64_t *`；系统码为 `uint16_t *`。
- `void mu3_io_led_set_colors(uint8_t board, const uint8_t *rgb)`：board1，立即复制18字节，其他board或空指针不发送。
- `uint32_t bridge_encode_led(uint8_t board, const uint8_t *rgb, uint8_t brightness, uint8_t *report, uint64_t report_size)`：编码65字节，允许输入输出重叠；无硬件访问。
- `uint32_t bridge_shutdown(uint32_t timeout_ms)`：请求停止并等待；超时返回258，不提前销毁仍被worker使用的资源。

找不到原DLL/出口时返回0、失败 HRESULT或中性输入，具体依各签名；这不能证明安装正确。初始化只尝试一次，补齐文件后应重启进程，不支持热加载。

## 灯光报文

所有偏移都从0起，RGB字节不做gamma/通道重排。

| Windows report偏移 | firmware payload偏移 | 内容 |
| --- | --- | --- |
| 0 | 不属于payload | ReportID前缀0 |
| 1 | 0 | Type 0（灯光） |
| 2 | 1 | brightness，最大40 |
| 3–11 | 2–10 | RGB0–2，旧槽0–2 |
| 12–17 | 11–16 | 零，旧槽3–4 |
| 18–26 | 17–25 | RGB3–5，旧槽5–7 |
| 27–64 | 26–63 | 零 |

每帧先清零，避免传递未初始化padding；没有Type1卡写入分支。桥的64B payload与Windows65B report不要混淆。

## 设备与同步

使用Windows SetupAPI/HID接口枚举，匹配VID/PID/usage/input-output report长度以及所有input/output caps的ReportID。系统API实现并非本项目附带驱动。多个匹配设备时无稳定排序和显式选择。

- 回调仅覆盖latest frame，并唤醒writer；不会在调用线程等待USB写完成。
- worker持有设备、缓冲区及OVERLAPPED。单次等待100ms；超时或不完整结果取消并drain，不提前重用内存。
- 最大速率由34ms间隔控制（约29.4Hz）。连接失败退避约1秒。
- 同色空闲500ms释放设备和同会话命名互斥量，冷却1000ms，再打开并重发同色，避免永久占有/去重阻止重连。不是颜色优先级协议。
- 模块在首次配置初始化后pin，原DLL保留到进程退出；退出时不在DllMain等待线程。永久取消等待时宁可保留资源，不制造use-after-free。

见[测试与限制](testing.md)。
