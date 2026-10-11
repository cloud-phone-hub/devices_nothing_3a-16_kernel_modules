<!-- Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course) -->

# 3a-16 virtio-media 驱动源码

本目录保存 3a-16（Nothing Phone (3a) / Android 16）云机 guest 使用的 virtio-media 内核驱动源码。初始代码来自 `https://github.com/chromeos/virtio-media.git` 的 `v0.0.8` 标签，并保留上游 `BSD-3-Clause OR GPL-2.0+` 双许可证与原始版权声明。

设备仓在上游驱动基础上维护以下集成变化：

- Kbuild 输出名称固定为 `cdh_media.ko`，对应 `modinfo name=cdh_media`；
- Guest 通过模块参数 `allow_userptr=1` 启用 Android Codec2 所需的 USERPTR 队列（vendor_dlkm `modules.options` 由 build-vendor_dlkm 追加）；
- 宿主上报不可恢复的会话错误后，驱动保存首次错误并停止该会话后续 ioctl 与 mmap；
- 错误事件唤醒阻塞中的 DQBUF 与 poll，poll 回读 `EPOLLERR`；
- 会话内存、描述符表和 V4L2 文件状态保留到应用关闭文件时再释放，避免仍在运行的 Codec2 线程访问已释放资源；终止错误已经关闭宿主会话，因此文件关闭时不重复发送命令。

事件处理与文件关闭均使用设备命令锁保护会话生命周期，事件锁只在设备命令锁内部获取。订阅初始 V4L2 事件时调用已持锁入口，异步事件工作线程则先获取设备命令锁，从而保持固定锁序。

## 构建与发布

设备仓统一构建（`kernel/build/build-kernel.sh` 从当前内核工作树与 `kernel/modules/*` 构建全部模块）：

```text
cd devices/nothing/3a-16
python3 build/scripts/build-kernel.py
```

当前构建配方为 `lineage-23-gki-virtio-wifi-cdh-modules-v3`，发布模块必须带 `6.12.23-4k-cdh` vermagic（Android16 GKI 6.12），并与同目录 `Module.symvers` 完成 MODPOST。

产物发布到 `out/kernel/modules/cdh_media.ko`，随 `build-vendor_dlkm.py` 打包进 vendor_dlkm.img，由发布目录 `SHA256SUMS` 校验。宿主侧由 qemu/media-host-* 后端提供编解码服务；云机规格见 `core/host/daemon/device/device.go` 的 `ThreeA16`（`HardwareMedia: true`）。
