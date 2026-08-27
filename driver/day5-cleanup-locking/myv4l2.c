#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

static struct v4l2_device v4l2_dev;
static struct video_device *vdev;
static struct mutex myv4l2_lock;

static int myv4l2_open(struct file *file)
{
    printk(KERN_INFO "myv4l2: open() called\n");
    return v4l2_fh_open(file);
}

static int myv4l2_release(struct file *file)
{
    printk(KERN_INFO "myv4l2: release() called\n");
    return v4l2_fh_release(file);
}

static const struct v4l2_file_operations myv4l2_fops = {
    .owner = THIS_MODULE,
    .open = myv4l2_open,
    .release = myv4l2_release,
    .unlocked_ioctl = video_ioctl2,
};

static int vidioc_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
    printk(KERN_INFO "myv4l2: VIDIOC_QUERYCAP called\n");
    strscpy(cap->driver, "myv4l2driver", sizeof(cap->driver));
    strscpy(cap->card, "My Synthetic V4L2 Camera", sizeof(cap->card));
    strscpy(cap->bus_info, "virtual", sizeof(cap->bus_info));
    cap->device_caps = V4L2_CAP_VIDEO_CAPTURE;
    cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
    return 0;
}

static const struct v4l2_ioctl_ops myv4l2_ioctl_ops = {
    .vidioc_querycap = vidioc_querycap,
};

static int __init myv4l2_init(void)
{
    int ret;

    printk(KERN_INFO "myv4l2: init starting\n");

    mutex_init(&myv4l2_lock);

    strscpy(v4l2_dev.name, "myv4l2driver", sizeof(v4l2_dev.name));

    ret = v4l2_device_register(NULL, &v4l2_dev);
    if (ret) {
        printk(KERN_ERR "myv4l2: v4l2_device_register failed: %d\n", ret);
        return ret;
    }
    printk(KERN_INFO "myv4l2: v4l2_device_register OK\n");

    vdev = video_device_alloc();
    if (!vdev) {
        printk(KERN_ERR "myv4l2: video_device_alloc failed\n");
        v4l2_device_unregister(&v4l2_dev);
        return -ENOMEM;
    }
    printk(KERN_INFO "myv4l2: video_device_alloc OK\n");

    strscpy(vdev->name, "myv4l2device", sizeof(vdev->name));
    vdev->v4l2_dev = &v4l2_dev;
    vdev->fops = &myv4l2_fops;
    vdev->ioctl_ops = &myv4l2_ioctl_ops;
    vdev->release = video_device_release;
    vdev->lock = &myv4l2_lock;
    vdev->vfl_dir = VFL_DIR_RX;
    vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE;

    ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
    if (ret) {
        printk(KERN_ERR "myv4l2: video_register_device failed: %d\n", ret);
        video_device_release(vdev);
        v4l2_device_unregister(&v4l2_dev);
        return ret;
    }

    printk(KERN_INFO "myv4l2: registered as /dev/video%d\n", vdev->num);
    return 0;
}

static void __exit myv4l2_exit(void)
{
    printk(KERN_INFO "myv4l2: exit starting\n");
    video_unregister_device(vdev);
    v4l2_device_unregister(&v4l2_dev);
    printk(KERN_INFO "myv4l2: unregistered\n");
}

module_init(myv4l2_init);
module_exit(myv4l2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Unnati");
MODULE_DESCRIPTION("Day 4 - V4L2 device registration with QUERYCAP");

