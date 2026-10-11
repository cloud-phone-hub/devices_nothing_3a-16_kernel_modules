# Device Info KO（3a-16 / Nothing Phone (3a)）

Android16 设备信息展示对齐模块。链接为单个 `cdh_device_info.ko`，覆盖 UTS/`uname(2)` 可见字段、`/proc/meminfo` capacity 展示字段、`sysinfo(2).uptime` / `/proc/uptime` 启动时长、`/proc/cpuinfo` 与 `cpu_capacity` 展示字段、只读 CPU cpufreq sysfs、`cpu_topology[]` 派生 topology sysfs、arm64 `cpu_data.reg_midr`（`midr_el1` sysfs 来源）以及 `cpuN/cache` 父设备隐藏。

## 边界

- 目标设备：`3a-16`（Nothing Phone (3a) / Asteroids / A059，board volcano = 骁龙 7s Gen 3 / SM7635，固件 `Asteroids_B4.1-260810-1153` / Android 16）。
- guest 内核 ABI：Android16 GKI 6.12；模块固定用 `ghcr.io/feicong/droid-ddk:android16-6.12` 构建，`vermagic=6.12.23-4k-cdh`。
- 用户态展示目标 `uname -r`：`6.1.157-android14-11-g82d681c9b06b-ab14634535`（真机内核代际）；ABI 校验目标：`gki_abi_release=6.12.23-4k-cdh`（云机实际内核）。两者刻意不同：展示面对齐真机，加载门禁对齐 guest。
- `init_uts_ns.name` 用真机官方内核串改写（release/version 双串），App、toybox `uname` 与 libc `uname(2)` 同源。
- `android_vh_si_meminfo_adjust` 改写 `si_meminfo()` 的 `totalram`；`android_vh_meminfo_proc_show` 只在 `/proc/meminfo` 已生成缓冲区内原位替换 `SwapTotal` / `CommitLimit` / `CmaTotal` / `VmallocTotal`，不追加重复字段。
- kretprobe 同时处理 `__arm64_sys_sysinfo` 与 `uptime_proc_show`，为两条 uptime 读取路径增加相同 offset（`uptime_offset_seconds`，默认 172800）。
- `android_rvh_cpuinfo_c_show` 伪造 8 个 CPU 的 `/proc/cpuinfo` 块（arm64 Android 无 `Hardware` 行）；`android_rvh_cpu_capacity_show` 输出 4+3+1 三簇容量（454/984/1024）。
- `get_cpu_device()` + 只读 `cpufreq` kobject 补 `cpuinfo_min/max_freq`、`scaling_min/max/cur_freq`、`scaling_governor`、`scaling_available_governors`、`affected_cpus`、`related_cpus`（virtio guest 无真实 cpufreq policy 目录）。
- 直接改写 `cpu_topology[]`（topology sysfs canonical source）：cluster0=cpu0-3、cluster1=cpu4-6、cluster2=cpu7，core_siblings=ff、cluster_cpus=0f/70/80、LLC=ff，卸载恢复。
- kprobe 解析未导出的 per-cpu `cpu_data`，写 `struct cpuinfo_arm64.reg_midr`（`midr_el1` sysfs canonical source，不建替代节点）。
- kprobe 解析私有 `cpu_cache_sysfs_exit` / `cacheinfo_cpu_online` / `cache_dev_map`，隐藏 `cpuN/cache` 父设备并在卸载时恢复；私有回调必须走 `__nocfi` 包装（GKI kCFI 对模块间间接调用会 panic）。
- 不改真实 boot image、`vermagic`、QEMU/HVF CPU 能力或 Android property source；不伪造 Free/抖动类字段（`MemFree`、`MemAvailable` 等只保证关系约束）。

## 取值依据与待真机取证项

镜像内已定案（值出自官方 `Asteroids_B4.1-260810-1153` 镜像，取证产物在 `out/tmp/`）：

| 项 | 值 | 来源 |
| --- | --- | --- |
| uname release/version | `6.1.157-android14-11-g82d681c9b06b-ab14634535` / `#1 SMP PREEMPT Mon Dec 22 19:22:33 UTC 2025` | 官方 boot.img 内内核 banner（`out/tmp/gold-kernel/Image` 的 `Linux version` 行），与官方 `goodix_fp.ko` vermagic 前缀 `6.1.157-android14-11` 互证 |
| 簇结构 | cpu0-3 + cpu4-6 + cpu7（4+3+1） | vendor_boot 内 Volcano base DTB `cpus/cpu-map`（cluster0 4 核、cluster1 3 核、cluster2 1 核），与 `/vendor/etc/perf/targetconfig.xml`（NumClusters=3、little4/big3/prime1、CoreCtlCpu=4）一致 |
| capacity 定标 | 454 / 984 / 1024 | base DTB `capacity-dmips-mhz` 0x400/0x686/0x686 按 6.12 公式（`raw × cpuinfo.max_freq` 归一到 1024） |
| cpufreq policy 号 | policy0 / policy4 / policy7 | 官方 `/vendor/etc/init/hw/init.nt_product.rc`、`/vendor/etc/perf/commonresourceconfigs.xml` 实际写的节点路径 |
| cpufreq 下限 | 441600 / 480000 / 480000 | base DTB `qcom,memlat` l3 各簇 `cpufreq-memfreq-tbl` 最低档 |
| cpufreq 上限 | 1804800 / 2400000 / 2496000 | 同表内实际最高 CPU 档（memlat 表上界 2054400/2707200/2918400 只是带宽投票子集，不作上限），与骁龙 7s Gen 3 公开档位一致 |
| 指纹注册面 | driver/class 均 `goodix_fp`、input 名 `qwerty`、13 键 | vendor_dlkm `goodix_fp.ko` 反汇编（见 `../goodix_fingerprint/README.md`） |

占位待真机复核（真机 `cat /proc/cpuinfo` / `midr_el1` sysfs / `meminfo` 取证后替换）：

- MIDR：implementer `0x41`（ARM）、part `0xd80`=Cortex-A520（cpu0-3）/ `0xd81`=Cortex-A720（cpu4-7），取内核 `arch/arm64/include/asm/cputype.h` 定义；variant/revision `0` 为占位（与 `cpu_display.c`、`midr_sysfs.c` 三口一致）。
- BogoMIPS `38.40`（base DTB `arm,armv8-timer` `clock-frequency=<0x124f800>` = 19.2MHz，2×）。
- Features 行：沿用 ARMv9 公共 HWCAP 集占位，未按 SM7635 真机逐项核对。
- governor：显示 `walt`，依据是官方 perf 配置写 `policyN/walt/*`（该目录只在 walt governor 的 policy 下存在）；`/vendor/etc/perf/targetconfig.xml` 的 `CpufreqGov="1"` 指 perf HAL 的 gov 实例类型，不作真机 `scaling_governor` 结论。真机取证后须同时校正 `scaling_available_governors`（现与 `scaling_governor` 同串）。
- meminfo：`MemTotal=7760724` 等 5 值为 8GB 档同级数值——上限受云机 guest 物理内存约束（device.go `ThreeA16 MemoryMB=8192`）；Nothing Phone (3a) 在售 8GB/12GB，真机取证或云机扩容后需同步上调（`CommitLimit` 满足 overcommit_ratio=50% 口径）。

## 源码组织

```text
core/    模块 init/exit、module_param 与统一调度
uts/     UTS namespace / uname 展示层
meminfo/ si_meminfo/sysinfo 与 /proc/meminfo capacity 展示层
time/    sysinfo(2).uptime 与 /proc/uptime 启动时长展示层
cpu/     /proc/cpuinfo、cpu_capacity、topology、MIDR、cache 与只读 cpufreq sysfs 展示层
include/ 模块内共享头文件
```

新增展示项时只追加子目录源码并更新 `Kbuild` 的 `$(MODULE_NAME)-objs`，最终仍链接为一个 `cdh_device_info.ko`。

## 构建

由设备仓统一构建（模块源码随内核一起进 builder 容器）：

```text
cd devices/nothing/3a-16
python3 build/scripts/build-kernel.py            # 内核 + 全部模块 + 归集 out/dist/kernel/modules
python3 build/scripts/build-kernel.py --dry-run  # 仅打印构建配置
```

产物：`out/kernel/modules/cdh_device_info.ko`。vermagic 校验：

```text
strings out/kernel/modules/cdh_device_info.ko | rg '^(description=|license=|vermagic=)'
# 期望 vermagic=6.12.23-4k-cdh
```

`insmod` 报 `Unknown symbol` / `Invalid module format` / `Exec format error` / UTS 或 MIDR `cpu_data` 解析失败时直接失败并归档 `dmesg`，不回退到 App/framework/native hook。
