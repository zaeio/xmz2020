#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <linux/fb.h>

#include "video.h"
#define CAP_BUF_NUM 4 //缓冲区大小
struct buffer
{
    void *start;
    size_t length;
};

static char *dev_name = "/dev/video0"; //摄像头设备名
static int camera_fd = -1;

static unsigned int n_buffers = 0;
struct v4l2_capability cap;     //视频设备的功能
struct v4l2_input vinput;       //视频输入信息
struct v4l2_format fmt;         //用来设置摄像头的视频制式、帧格式等
struct v4l2_requestbuffers req; //设备的缓存区，一般不超过5个
struct v4l2_buffer buf;         //驱动中的一帧
enum v4l2_buf_type type;
struct buffer *Vbuffers = NULL; //用户空间缓冲区

static int lcd_fd;
static char *lcd_buf;
struct fb_var_screeninfo vinfo;
static long screensize = 0;

int v4l2_open()
{
    camera_fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0); //打开设备
    //2.取得设备的capability，看看设备具有什么功能，比如是否具有视频输入,或者音频输入输出等。VIDIOC_QUERYCAP,struct v4l2_capability
    //获取摄像头参数
    if (ioctl(camera_fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        printf("<app> VIDIOC_QUERYCAP error!\n");
        return -1;
    }
}

void v4l2_close()
{
    int i;
unmap:
    for (i = 0; i < n_buffers; ++i)
        if (-1 == munmap(Vbuffers[i].start, Vbuffers[i].length))
            printf("munmap error");
    if (-1 == munmap(lcd_buf, screensize))
        printf("munmap error");
    if (-1 == ioctl(camera_fd, VIDIOC_STREAMOFF, &type))
        printf("VIDIOC_STREAMOFF");
    //关闭视频设备
    close(camera_fd);
}

void v4l2_set_format()
{
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 800;                       //宽，必须是16的倍数
    fmt.fmt.pix.height = 480;                      //高，必须是16的倍数
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565; //视频数据存储类型，LCD屏幕支持显示RGB565 因此改成此格式显示
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    /* 设置设备捕获视频的格式 */
    if (ioctl(camera_fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        printf("iformat not supported \n");
        close(camera_fd);
        return -1;
    }
}

int v4l2_request_buffers()
{
    memset(&req, 0, sizeof(req));
    req.count = CAP_BUF_NUM; //申请大小为4的缓冲区
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(camera_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        printf("<app> VIDIOC_REQBUFS error!\n");
        return -1;
    }

    if (req.count < 1)
        printf("<app> Insufficient buffer memory\n");

    Vbuffers = calloc(req.count, sizeof(*Vbuffers)); //内存中建立对应空间
    //将申请到的帧缓冲映射到用户空间，这样就可以直接操作采集到的帧了，而不必去复制。mmap
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        memset(&buf,0,sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        //把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
        if (ioctl(camera_fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            printf("<app> VIDIOC_QUERYBUF error!\n");
            return -1;
        }
        //映射到用户空间
        Vbuffers[n_buffers].length = buf.length;
        Vbuffers[n_buffers].start =
            mmap(NULL /* start anywhere */,
                 buf.length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 camera_fd, buf.m.offset); //通过mmap建立映射关系,返回映射区的起始地址

        if (MAP_FAILED == Vbuffers[n_buffers].start)
        {
            printf("<app> mmap failed\n");
            return -1;
        }
        memset(Vbuffers[n_buffers].start, 0, buf.length); //=====================这里有疑问=========================//
        int i;
        //将申请到的帧缓冲全部入队列，以便存放采集到的数据.VIDIOC_QBUF,struct v4l2_buffer
        for (i = 0; i < n_buffers; ++i)
        {
            struct v4l2_buffer buf;
            memset(&buf,0,sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            //把数据从缓存中读取出来
            if (-1 == ioctl(camera_fd, VIDIOC_QBUF, &buf)) //申请到的缓冲进入列队
                printf("<app> VIDIOC_QBUF failed\n");
        }
    }
}

static int read_frame(void)
{
    struct v4l2_buffer buf;
    unsigned int startx;
    unsigned int starty;
    int ret = -1;

    memset(&buf,0,sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    /*8.出队列以取得已采集数据的帧缓冲，取得原始采集数据。VIDIOC_DQBUF*/
    ret = ioctl(camera_fd, VIDIOC_DQBUF, &buf);
    if (ret < 0)
    {
        printf("error: VIDIOC_DQBUF %s\n", __func__);
        goto err;
    }

    assert(buf.index < n_buffers);

    int i, j;
    /*9.LCD屏幕显示*/
    startx = (vinfo.xres - fmt.fmt.pix.width) / 2;
    starty = (vinfo.yres - fmt.fmt.pix.height) / 2;
    for (i = 0; i < fmt.fmt.pix.height; i++)
    {
        for (j = 0; j < fmt.fmt.pix.width; j++)
        {
            ((unsigned short *)lcd_buf)[800 * (i + starty) + j + startx] = ((unsigned short *)(Vbuffers[buf.index].start))[fmt.fmt.pix.width * i + j];
        }
    }
    /*10.将缓冲重新入队列尾,这样可以循环采集。VIDIOC_QBUF*/
    ret = ioctl(camera_fd, VIDIOC_QBUF, &buf);
    if (ret < 0)
    {
        printf("error: VIDIOC_QBUF %s\n", __func__);
        goto err;
    }

err:
    return 1;
}

int v4l2_stream()
{
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(camera_fd, VIDIOC_STREAMON, &type)) //开始捕捉图像数据
    {
        printf("<app> VIDIOC_STREAMON failed\n");
        return -1;
    }
    for (;;) //这一段涉及到异步IO
    {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);           //将指定的文件描述符集清空
        FD_SET(camera_fd, &fds); //在文件描述符集合中增加新的文件描述符

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(camera_fd + 1, &fds, NULL, NULL, &tv); //判断是否可读（即摄像头是否准备好），tv是定时

        if (-1 == r)
        {
            if (EINTR == errno)
                continue;
            printf("select err\n");
        }

        if (0 == r)
        {
            fprintf(stderr, "select timeout\n");
            exit(EXIT_FAILURE);
        }
        read_frame();
        //usleep(100000);
    }
}
