#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ioctl.h>
#define MYV4L2_FRAME_WIDTH  640
#define MYV4L2_FRAME_HEIGHT 480
#define MYV4L2_FRAME_SIZE   (MYV4L2_FRAME_WIDTH * MYV4L2_FRAME_HEIGHT * 2)  /* placeholder: assume 2 bytes/pixel */

static struct v4l2_device v4l2_dev;
static struct video_device *vdev;
static struct vb2_queue vb2_q;
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

static int queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
                       unsigned int *nplanes, unsigned int sizes[],
                       struct device *alloc_devs[])
{
  if (*nbuffers < 2)
     *nbuffers = 2;

  if (*nplanes)
     return sizes[0] < MYV4L2_FRAME_SIZE ? -EINVAL : 0;

  *nplanes = 1;
  sizes[0] = MYV4L2_FRAME_SIZE;
  printk(KERN_INFO "myv4l2: queue_setup - %d buffers of size %d\n",*nbuffers, sizes[0]);
  
  return 0;
}
static int buffer_prepare(struct vb2_buffer *vb)
{
    unsigned long size = MYV4L2_FRAME_SIZE;
   
    if(vb2_plane_size(vb, 0) < size) {
       printk(KERN_ERR "myv4l2: buffer too small (%lu < %lu)\n", vb2_plane_size(vb, 0), size);
       return -EINVAL;
     }
    vb2_set_plane_payload(vb, 0, size);
    printk(KERN_INFO "myv4l2: buffer_prepare - buffer ready, size %lu\n", size);
    return 0;   

}

static void buffer_queue(struct vb2_buffer *vb)
{
    printk(KERN_INFO "myv4l2: buffer_queue called\n");
}

static const struct vb2_ops myv4l2_vb2_ops = {
    .queue_setup = queue_setup,
    .buf_prepare = buffer_prepare,
    .buf_queue = buffer_queue,
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
    printk(KERN_INFO "myv4l2: video_register_device OK\n");

    vb2_q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vb2_q.io_modes = VB2_MMAP;
    vb2_q.drv_priv = NULL;
    vb2_q.buf_struct_size = sizeof(struct vb2_buffer);
    vb2_q.ops = &myv4l2_vb2_ops;
    vb2_q.mem_ops = &vb2_vmalloc_memops;
    vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
    vb2_q.lock = &myv4l2_lock;

    ret = vb2_queue_init(&vb2_q);
    if (ret) {
        printk(KERN_ERR "myv4l2: failed to init vb2 queue: %d\n", ret);
        video_unregister_device(vdev);
        v4l2_device_unregister(&v4l2_dev);
        return ret;
    }
    printk(KERN_INFO "myv4l2: vb2_queue_init OK\n");

    vdev->queue = &vb2_q;
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
MODULE_DESCRIPTION("Day 6 - vb2 queue scaffold with buf_queue callback");

