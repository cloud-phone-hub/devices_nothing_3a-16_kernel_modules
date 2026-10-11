# Goodix Fingerprint KO（3a-16 / Nothing Phone (3a)）

指纹注册面只读枚举 KO。Nothing Phone (3a) 指纹为 Goodix 方案（与显示/触摸一体，SPI 挂载），真机注册面由官方 `goodix_fp.ko` 提供；云机 vendor_dlkm 为自产模块集、不含官方 `goodix_fp.ko`，本模块只补 kernel 可见注册面（AIDA64/DevInfo 类读取路径），不提供真实指纹能力、不发送任何 input event。

## 注册面（对 vendor_dlkm/goodix_fp.ko 反汇编定案）

取证方法：`llvm-objdump -d -r --symbolize-operands` 读调用点寄存器实参 + 手写 ELF 段表解析把 `.rodata.str1.1` 偏移还原成字符串（产物 `out/tmp/gold-ko/goodix_fp.ko`、`/tmp/gfp.S`）。

| 项 | 真机值 | 证据 | 实现 |
| --- | --- | --- | --- |
| platform driver | `goodix_fp` | `.init.text` `__platform_driver_register(&drv)`，`drv.driver.name` 重定位指向 `goodix_fp` | 空 platform_driver，生成 `/sys/bus/platform/drivers/goodix_fp` |
| class | `goodix_fp` | `.init.text` `__class_create(THIS_MODULE, "goodix_fp")` | `class_create("goodix_fp")`，生成 `/sys/class/goodix_fp` |
| input 设备名 | `qwerty` | `.text` probe 内 `str x9,[input_dev]`（`name` 偏移 0），实参指向 `qwerty` | `input_register_device` |
| input phys / id | 未赋值（phys=NULL，bustype/vendor/product/version 全 0） | probe 里除 `name` 外无字段写入 | 本模块同样不写 `phys`/`id` |
| 按键 capability | HOME MENU BACK POWER UP DOWN RIGHT LEFT CAMERA VOLUMEDOWN VOLUMEUP SEARCH CHAT（13 个） | probe 内 13 次 `input_set_capability(EV_KEY, code)`，code 依次为 102/139/158/116/103/108/106/105/212/114/115/217/216 | 同序同集 |
| DT 节点 | compatible `goodix,fingerprint`，GPIO 名 `fp-gpio-reset` / `fp-gpio-irq` | `.rodata` of_match_table 与 `of_get_named_gpio_flags` 实参 | 仅在官方 DT 中，guest DT 不需补 |
| char device | `__register_chrdev(0,0,256,"goodix_fp_spi",fops)`，probe 里 `device_create(..., "goodix_fp")` → `/dev/goodix_fp` | `.init.text` / `.text` 调用点 | **本模块不建 cdev/devnode**，见下 |

## 边界

- 不注册官方那样的 cdev（`goodix_fp_spi` 号段、`/dev/goodix_fp` 节点）——真机指纹 HAL 才 open 它，云机无该 HAL；若要补 `/proc/devices` 与 devnode 可见面，需同时给 `/dev/goodix_fp` 补 file_contexts 标签，等指纹链路联调再决策。
- 不注册 platform device、不触发真实硬件 probe（guest DT 无 `goodix,fingerprint` 节点，driver 目录为空即预期）。
- 不使用 uinput daemon、framework/native hook、kallsyms、kprobe、ftrace、syscall hook 或 text patch。
- 不发送 `KEY_POWER`、`KEY_HOME` 或任何其它 input event；不影响 QEMU tablet/mouse 或 Android input dispatcher。

## 构建

由设备仓统一构建：

```text
cd devices/nothing/3a-16
python3 build/scripts/build-kernel.py
```

产物：`out/kernel/modules/cdh_goodix_fingerprint.ko`（`vermagic=6.12.23-4k-cdh`）。

## 验证

经 web 控制台起实例后（本机 adb 端口以实例为准，如 5927）：

```text
adb -s 127.0.0.1:5927 shell 'cat /proc/modules | grep cdh_goodix_fingerprint'
adb -s 127.0.0.1:5927 shell 'ls -ld /sys/bus/platform/drivers/goodix_fp /sys/class/goodix_fp'
adb -s 127.0.0.1:5927 shell 'getevent -il | sed -n "/qwerty/,+30p"'
adb -s 127.0.0.1:5927 shell 'dumpsys input | grep -A30 -B5 qwerty'
```

预期：driver/class 目录存在；`getevent -il` 与 `dumpsys input` 能看到 `qwerty`（id 0000:0000、bus 0）与 13 个 EV_KEY；无事件注入。
