#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ioctl.h>

#define MYV4L2_FRAME_WIDTH  640
#define MYV4L2_FRAME_HEIGHT 480
#define MYV4L2_FRAME_SIZE   (MYV4L2_FRAME_WIDTH * MYV4L2_FRAME_HEIGHT * 2)

static struct v4l2_device v4l2_dev;
static struct video_device *vdev;
static struct vb2_queue vb2_q;
static struct mutex myv4l2_lock;
static struct v4l2_pix_format myv4l2_format;

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
    .mmap = vb2_fop_mmap,
    .poll = vb2_fop_poll,
};

static int vidioc_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
    printk(KERN_INFO "myv4l2: VIDIOC_QUERYCAP called\n");
    strscpy(cap->driver, "myv4l2driver", sizeof(cap->driver));
    strscpy(cap->card, "My Synthetic V4L2 Camera", sizeof(cap->card));
    strscpy(cap->bus_info, "virtual", sizeof(cap->bus_info));
    cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
    return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
    if (f->index != 0)
        return -EINVAL;
    f->pixelformat = V4L2_PIX_FMT_YUYV;
    strscpy(f->description, "YUYV 4:2:2", sizeof(f->description));
    return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
    f->fmt.pix.width = MYV4L2_FRAME_WIDTH;
    f->fmt.pix.height = MYV4L2_FRAME_HEIGHT;
    f->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    f->fmt.pix.field = V4L2_FIELD_NONE;
    f->fmt.pix.bytesperline = f->fmt.pix.width * 2;
    f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;
    f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
    return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
    f->fmt.pix = myv4l2_format;
    return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
    int ret;

    ret = vidioc_try_fmt_vid_cap(file, priv, f);
    if (ret)
        return ret;

    if (vb2_is_busy(&vb2_q)) {
        printk(KERN_ERR "myv4l2: cannot change format, queue is busy\n");
        return -EBUSY;
    }

    myv4l2_format = f->fmt.pix;
    return 0;
}

static const struct v4l2_ioctl_ops myv4l2_ioctl_ops = {
    .vidioc_querycap = vidioc_querycap,
    .vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
    .vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
    .vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
    .vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
    .vidioc_reqbufs = vb2_ioctl_reqbufs,
    .vidioc_querybuf = vb2_ioctl_querybuf,
    .vidioc_qbuf = vb2_ioctl_qbuf,
    .vidioc_dqbuf = vb2_ioctl_dqbuf,
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
    printk(KERN_INFO "myv4l2: queue_setup - %d buffers of size %d\n", *nbuffers, sizes[0]);
    return 0;
}

static int buffer_prepare(struct vb2_buffer *vb)
{
    unsigned long size = MYV4L2_FRAME_SIZE;

    if (vb2_plane_size(vb, 0) < size) {
        printk(KERN_ERR "myv4l2: buffer too small (%lu < %lu)\n", vb2_plane_size(vb, 0), size);
        return -EINVAL;
    }
    vb2_set_plane_payload(vb, 0, size);
    return 0;
}

static void buffer_queue(struct vb2_buffer *vb)
{
    printk(KERN_INFO "myv4l2: buffer_queue called, index=%d\n", vb->index);
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

    myv4l2_format.width = MYV4L2_FRAME_WIDTH;
    myv4l2_format.height = MYV4L2_FRAME_HEIGHT;
    myv4l2_format.pixelformat = V4L2_PIX_FMT_YUYV;
    myv4l2_format.field = V4L2_FIELD_NONE;
    myv4l2_format.bytesperline = MYV4L2_FRAME_WIDTH * 2;
    myv4l2_format.sizeimage = myv4l2_format.bytesperline * MYV4L2_FRAME_HEIGHT;
    myv4l2_format.colorspace = V4L2_COLORSPACE_SRGB;

    strscpy(v4l2_dev.name, "myv4l2driver", sizeof(v4l2_dev.name));

    ret = v4l2_device_register(NULL, &v4l2_dev);
    if (ret) {
        printk(KERN_ERR "myv4l2: v4l2_device_register failed: %d\n", ret);
        return ret;
    }

    vdev = video_device_alloc();
    if (!vdev) {
        v4l2_device_unregister(&v4l2_dev);
        return -ENOMEM;
    }

    strscpy(vdev->name, "myv4l2device", sizeof(vdev->name));
    vdev->v4l2_dev = &v4l2_dev;
    vdev->fops = &myv4l2_fops;
    vdev->ioctl_ops = &myv4l2_ioctl_ops;
    vdev->release = video_device_release;
    vdev->lock = &myv4l2_lock;
    vdev->vfl_dir = VFL_DIR_RX;
    vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

    ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
    if (ret) {
        video_device_release(vdev);
        v4l2_device_unregister(&v4l2_dev);
        return ret;
    }

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

    vdev->queue = &vb2_q;
    printk(KERN_INFO "myv4l2: registered as /dev/video%d\n", vdev->num);
    return 0;
}

static void __exit myv4l2_exit(void)
{
    video_unregister_device(vdev);
    v4l2_device_unregister(&v4l2_dev);
    printk(KERN_INFO "myv4l2: unregistered\n");
}

module_init(myv4l2_init);
module_exit(myv4l2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Unnati");
MODULE_DESCRIPTION("Day 10 - REQBUFS/QUERYBUF/QBUF/DQBUF via vb2 helpers");
