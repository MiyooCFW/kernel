#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sysfs.h>

#define SYSFS_DIR "miyoo_video"
#define SYSFS_FILE "miyoo_video"

static struct kobject *video_kobj;
static char *driver = "";

module_param(driver, charp, S_IRUGO);
MODULE_PARM_DESC(driver, "Name of video driver to load");

static int replacechar(char *str, char orig, char rep) {
    char *ix = str;
    int n = 0;
    while((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}

static ssize_t miyoo_video_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	replacechar(driver, '+', ' ');
    return sprintf(buf, "%s", driver); 
}

static ssize_t miyoo_video_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    pr_info("Received %zu bytes: %s\n", count, buf);
    return count;
}

static struct kobj_attribute miyoo_video_attribute =
    __ATTR(miyoo_video, 0664, miyoo_video_show, miyoo_video_store);

static int __init sysfs_miyoo_init(void)
{
    int result;

    pr_info("Miyoo video driver name module loaded\n");

    video_kobj = kobject_create_and_add(SYSFS_DIR, kernel_kobj);
    if (!video_kobj)
    {
        pr_err("Failed to create sysfs directory\n");
        return -ENOMEM;
    }

    result = sysfs_create_file(video_kobj, &miyoo_video_attribute.attr);
    if (result)
    {
        pr_err("Failed to create sysfs file\n");
        kobject_put(video_kobj);
        return result;
    }

    return 0;
}

static void __exit sysfs_miyoo_exit(void)
{
    pr_info("Miyoo video driver name module unloaded\n");

    if (video_kobj)
    {
        kobject_put(video_kobj);
        sysfs_remove_file(kernel_kobj, &miyoo_video_attribute.attr);
    }
}

module_init(sysfs_miyoo_init);
module_exit(sysfs_miyoo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tiopex tiopxyz@gmail.com");
MODULE_DESCRIPTION("Kernel module with a name of video driver from uboot");
