// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
/*
 * Nothing Phone (3a)（volcano / SM7635）指纹 = Goodix 光学方案，与显示触摸一体；
 * 真机注册面由官方 goodix_fp.ko 提供（对 vendor_dlkm/goodix_fp.ko 反汇编定案）：
 *   - module init: __register_chrdev(0,0,256,"goodix_fp_spi",fops)
 *   - module init: __class_create(THIS_MODULE,"goodix_fp") → class "goodix_fp"
 *   - module init: __platform_driver_register(&drv)，drv.driver.name="goodix_fp"
 *   - probe: device_create(class, parent, MKDEV(major,i), .., "goodix_fp")
 *     → /dev/goodix_fp（cdev 由官方驱动提供，本模块不建）
 *   - probe: input_dev 只设 name="qwerty"（phys/id 均未赋值，bustype/vendor/
 *     product/version 全 0），13 个 EV_KEY capability
 *   - DT 节点 compatible "goodix,fingerprint"，GPIO 名 fp-gpio-reset/fp-gpio-irq
 * 云机 vendor_dlkm 为自产模块集、不含官方 goodix_fp.ko，本模块只补上述可见
 * 注册面（不发输入事件、不实现指纹功能、不建 cdev/devnode）。
 */
#include <linux/device/class.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define CDH_FP_DRIVER_NAME "goodix_fp"
#define CDH_FP_CLASS_NAME "goodix_fp"
#define CDH_FP_INPUT_NAME "qwerty"

static struct input_dev *cdh_fp_input;
static struct class *cdh_fp_class;

static struct platform_driver cdh_fp_platform_driver = {
    .driver =
        {
            .name = CDH_FP_DRIVER_NAME,
            .owner = THIS_MODULE,
        },
};

/* 按键集与顺序取自 goodix_fp.ko 的 input_set_capability 调用点。 */
static const unsigned int cdh_fp_keys[] = {
    KEY_HOME,   KEY_MENU,   KEY_BACK,   KEY_POWER, KEY_UP,
    KEY_DOWN,   KEY_RIGHT,  KEY_LEFT,   KEY_CAMERA, KEY_VOLUMEDOWN,
    KEY_VOLUMEUP, KEY_SEARCH, KEY_CHAT,
};

static int cdh_fp_register_input(void) {
  struct input_dev *input;
  int ret;
  size_t i;

  input = input_allocate_device();
  if (!input)
    return -ENOMEM;

  /* 官方驱动只赋 name，其余字段留零，故此处不写 phys/id。 */
  input->name = CDH_FP_INPUT_NAME;

  for (i = 0; i < ARRAY_SIZE(cdh_fp_keys); i++)
    input_set_capability(input, EV_KEY, cdh_fp_keys[i]);

  ret = input_register_device(input);
  if (ret) {
    input_free_device(input);
    return ret;
  }

  cdh_fp_input = input;
  return 0;
}

static void cdh_fp_unregister_input(void) {
  if (!cdh_fp_input)
    return;
  input_unregister_device(cdh_fp_input);
  cdh_fp_input = NULL;
}

static int __init cdh_fp_init(void) {
  int ret;

  ret = platform_driver_register(&cdh_fp_platform_driver);
  if (ret)
    return ret;

  cdh_fp_class = class_create(CDH_FP_CLASS_NAME);
  if (IS_ERR(cdh_fp_class)) {
    ret = PTR_ERR(cdh_fp_class);
    cdh_fp_class = NULL;
    goto err_unregister_driver;
  }

  ret = cdh_fp_register_input();
  if (ret)
    goto err_class_destroy;

  pr_info("cdh_goodix_fingerprint: registered %s, %s and %s\n",
          CDH_FP_DRIVER_NAME, CDH_FP_CLASS_NAME, CDH_FP_INPUT_NAME);
  return 0;

err_class_destroy:
  class_destroy(cdh_fp_class);
  cdh_fp_class = NULL;
err_unregister_driver:
  platform_driver_unregister(&cdh_fp_platform_driver);
  return ret;
}

static void __exit cdh_fp_exit(void) {
  cdh_fp_unregister_input();
  if (cdh_fp_class) {
    class_destroy(cdh_fp_class);
    cdh_fp_class = NULL;
  }
  platform_driver_unregister(&cdh_fp_platform_driver);
  pr_info("cdh_goodix_fingerprint: removed\n");
}

module_init(cdh_fp_init);
module_exit(cdh_fp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fei_cong <fei_cong@hotmail.com>");
MODULE_DESCRIPTION("Read-only Android16 Nothing goodix_fp (Goodix) sysfs/input "
                   "enumerator for CDH");
