// Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sysfs.h>

#define CDH_CPU_PROC_NAME "cdh_cpu_profile"
#define CDH_CPU_PROC_PATH "/proc/cdh_cpu_profile"
#define CDH_CPU_SYSFS_NAME "cdh_cpu"
#define CDH_CPU_SYSFS_PATH "/sys/kernel/cdh_cpu"

static struct proc_dir_entry *cdh_cpu_proc;
static struct kobject *cdh_cpu_kobj;

/*
 * 本文件是模块自述画像（供联调核对），值与 device_info 运行时展示保持一致；
 * MIDR part/variant 与 meminfo 容量等占位项见 device_info 注释。
 */
static const char cdh_cpu_profile[] =
	"device=3a-16\n"
	"android=16\n"
	"source= Nothing Phone (3a) (Asteroids/A059, volcano/SM7635) Asteroids_B4.1-260810-1153 official images\n"
	"features=fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp sha3 asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp flagm2 frint i8mm bf16 dgh rng bti\n"
	"cluster0=cpu0-3 cortex-a520 capacity=454 min=441600 max=1804800 midr=0x000000004100d800\n"
	"cluster1=cpu4-6 cortex-a720 capacity=984 min=480000 max=2400000 midr=0x000000004100d810\n"
	"cluster2=cpu7 cortex-a720 capacity=1024 min=480000 max=2496000 midr=0x000000004100d810\n";

static const char cdh_cpu_clusters[] =
	"cpu0-3 affected=0 1 2 3 related=0 1 2 3 capacity=454\n"
	"cpu4-6 affected=4 5 6 related=4 5 6 capacity=984\n"
	"cpu7 affected=7 related=7 capacity=1024\n";

static const char cdh_cpu_midr_el1[] =
	"cpu0=0x000000004100d800\n"
	"cpu1=0x000000004100d800\n"
	"cpu2=0x000000004100d800\n"
	"cpu3=0x000000004100d800\n"
	"cpu4=0x000000004100d810\n"
	"cpu5=0x000000004100d810\n"
	"cpu6=0x000000004100d810\n"
	"cpu7=0x000000004100d810\n";

static const char cdh_cpu_cpufreq[] =
	"cpu0-3 governor=walt min=441600 max=1804800 cur=1804800\n"
	"cpu4-6 governor=walt min=480000 max=2400000 cur=2400000\n"
	"cpu7 governor=walt min=480000 max=2496000 cur=2496000\n";

static int cdh_cpu_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, cdh_cpu_profile);
	return 0;
}

static int cdh_cpu_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cdh_cpu_proc_show, NULL);
}

static const struct proc_ops cdh_cpu_proc_ops = {
	.proc_open = cdh_cpu_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t profile_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", cdh_cpu_profile);
}

static ssize_t clusters_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", cdh_cpu_clusters);
}

static ssize_t midr_el1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", cdh_cpu_midr_el1);
}

static ssize_t cpufreq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", cdh_cpu_cpufreq);
}

static struct kobj_attribute profile_attr = __ATTR_RO(profile);
static struct kobj_attribute clusters_attr = __ATTR_RO(clusters);
static struct kobj_attribute midr_el1_attr = __ATTR_RO(midr_el1);
static struct kobj_attribute cpufreq_attr = __ATTR_RO(cpufreq);

static struct attribute *cdh_cpu_attrs[] = {
	&profile_attr.attr,
	&clusters_attr.attr,
	&midr_el1_attr.attr,
	&cpufreq_attr.attr,
	NULL,
};

static const struct attribute_group cdh_cpu_attr_group = {
	.attrs = cdh_cpu_attrs,
};

static int __init cdh_cpu_probe_init(void)
{
	int ret;

	cdh_cpu_proc = proc_create(CDH_CPU_PROC_NAME, 0444, NULL, &cdh_cpu_proc_ops);
	if (!cdh_cpu_proc)
		return -ENOMEM;

	cdh_cpu_kobj = kobject_create_and_add(CDH_CPU_SYSFS_NAME, kernel_kobj);
	if (!cdh_cpu_kobj) {
		proc_remove(cdh_cpu_proc);
		cdh_cpu_proc = NULL;
		return -ENOMEM;
	}

	ret = sysfs_create_group(cdh_cpu_kobj, &cdh_cpu_attr_group);
	if (ret) {
		kobject_put(cdh_cpu_kobj);
		cdh_cpu_kobj = NULL;
		proc_remove(cdh_cpu_proc);
		cdh_cpu_proc = NULL;
		return ret;
	}

	pr_info("cdh_cpu_probe: registered %s and %s\n",
		CDH_CPU_PROC_PATH, CDH_CPU_SYSFS_PATH);
	return 0;
}

static void __exit cdh_cpu_probe_exit(void)
{
	if (cdh_cpu_kobj) {
		sysfs_remove_group(cdh_cpu_kobj, &cdh_cpu_attr_group);
		kobject_put(cdh_cpu_kobj);
		cdh_cpu_kobj = NULL;
	}
	if (cdh_cpu_proc) {
		proc_remove(cdh_cpu_proc);
		cdh_cpu_proc = NULL;
	}
	pr_info("cdh_cpu_probe: removed\n");
}

module_init(cdh_cpu_probe_init);
module_exit(cdh_cpu_probe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fei_cong <fei_cong@hotmail.com>");
MODULE_DESCRIPTION("Read-only Android16 CPU profile probe for CDH");
