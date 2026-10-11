// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 */
#define pr_fmt(fmt) "cdh_device_info_cpu: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <trace/hooks/cpuinfo.h>
#include <trace/hooks/topology.h>

#include "cdh_device_info.h"

/*
 * Features 取 ARMv9 公共 HWCAP 集占位（未按 SM7635 逐项核对）；待真机
 * /proc/cpuinfo 复核后按真机行原样替换。
 */
static const char cdh_cpu_features[] =
	"fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp sha3 asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint i8mm bf16 dgh rng bti";

struct cdh_cpu_core {
	unsigned int cpu;
	const char *bogo_mips;
	const char *implementer;
	unsigned int variant;
	const char *part;
	unsigned int revision;
	unsigned long capacity;
};

static const struct cdh_cpu_core cdh_cpu_cores[] = {
	/*
	 * Nothing Phone (3a)（volcano / SM7635 = 骁龙 7s Gen 3）4+3+1：
	 * cpu0-3 Cortex-A520、cpu4-6 Cortex-A720、cpu7 Cortex-A720。
	 * BogoMIPS 38.40 = 2 × 19.2MHz arch timer（Volcano DTB timer
	 * clock-frequency=<0x124f800>）。
	 * implementer 0x41=ARM；part 0xd80=Cortex-A520、0xd81=Cortex-A720
	 * （内核 arch/arm64/include/asm/cputype.h 定义），variant/revision
	 * 真机镜像内无取证，先按 0 占位并与 midr_sysfs.c 保持一致。
	 * capacity 为 6.12 公式对 DTB capacity-dmips-mhz(silver 0x400 /
	 * gold+prime 0x686) 与各簇 cpuinfo.max_freq(1804800/2400000/2496000)
	 * 的定标结果：454 / 984 / 1024。
	 */
	{ 0, "38.40", "0x41", 0x0, "0xd80", 0, 454 },
	{ 1, "38.40", "0x41", 0x0, "0xd80", 0, 454 },
	{ 2, "38.40", "0x41", 0x0, "0xd80", 0, 454 },
	{ 3, "38.40", "0x41", 0x0, "0xd80", 0, 454 },
	{ 4, "38.40", "0x41", 0x0, "0xd81", 0, 984 },
	{ 5, "38.40", "0x41", 0x0, "0xd81", 0, 984 },
	{ 6, "38.40", "0x41", 0x0, "0xd81", 0, 984 },
	{ 7, "38.40", "0x41", 0x0, "0xd81", 0, 1024 },
};

static bool cdh_cpuinfo_registered;
static bool cdh_cpu_capacity_registered;
static bool cdh_cpu_display_pinned;

static const struct cdh_cpu_core *cdh_cpu_target(unsigned int cpu)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cdh_cpu_cores); i++) {
		if (cdh_cpu_cores[i].cpu == cpu)
			return &cdh_cpu_cores[i];
	}
	return NULL;
}

static void cdh_cpuinfo_show(void *data, struct seq_file *m)
{
	size_t i;

	m->count = 0;
	m->from = 0;
	for (i = 0; i < ARRAY_SIZE(cdh_cpu_cores); i++) {
		const struct cdh_cpu_core *core = &cdh_cpu_cores[i];

		seq_printf(m, "processor\t: %u\n", core->cpu);
		seq_printf(m, "BogoMIPS\t: %s\n", core->bogo_mips);
		seq_printf(m, "Features\t: %s\n", cdh_cpu_features);
		seq_printf(m, "CPU implementer\t: %s\n", core->implementer);
		seq_puts(m, "CPU architecture: 8\n");
		seq_printf(m, "CPU variant\t: 0x%x\n", core->variant);
		seq_printf(m, "CPU part\t: %s\n", core->part);
		seq_printf(m, "CPU revision\t: %u\n\n", core->revision);
	}
	/* 真机 arm64 Android /proc/cpuinfo 无 Hardware 行，保持与真机一致不输出。 */
}

static void cdh_cpu_capacity_show(void *data, unsigned long *capacity, int cpu)
{
	const struct cdh_cpu_core *core;

	if (!capacity || cpu < 0)
		return;
	core = cdh_cpu_target((unsigned int)cpu);
	if (!core)
		return;
	*capacity = core->capacity;
}

int cdh_cpu_display_init(void)
{
	int ret;

	ret = register_trace_android_rvh_cpuinfo_c_show(cdh_cpuinfo_show, NULL);
	if (ret)
		return ret;
	cdh_cpuinfo_registered = true;

	ret = register_trace_android_rvh_cpu_capacity_show(cdh_cpu_capacity_show, NULL);
	if (ret) {
		pr_err("registered /proc/cpuinfo hook but cpu_capacity hook failed:%d\n", ret);
		__module_get(THIS_MODULE);
		cdh_cpu_display_pinned = true;
		return 0;
	}
	cdh_cpu_capacity_registered = true;

	__module_get(THIS_MODULE);
	cdh_cpu_display_pinned = true;
	pr_info("registered permanent /proc/cpuinfo and cpu_capacity display hooks\n");
	return 0;
}

void cdh_cpu_display_exit(void)
{
	if (!cdh_cpuinfo_registered && !cdh_cpu_capacity_registered)
		return;

	pr_info("restricted CPU display hooks are permanent; pinned=%d\n",
		cdh_cpu_display_pinned);
}
