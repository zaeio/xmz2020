#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include "ak_common.h"
#include "ak_log.h"
#include "ak_common_video.h"
#include "ak_venc.h"
#include "ak_thread.h"
#include "ak_mem.h"
#include "ak_vi.h"
#include "rtp1.h"

#ifdef AK_RTOS
#include "rtthread.h"
#define THREAD_PRIO 90
#else
#define THREAD_PRIO -1
#endif

#define RECORD_READ_LEN 4096 * 10 /* read video file buffer length */
#define DE_VIDEO_SIZE_MAX 6
/* this is length for parse */
#define LEN_HINT 512
#define LEN_OPTION_SHORT 512

/* mipi screen res */
#define MIPI_SC_WIDTH 1280
#define MIPI_SC_HEIGHT 800

/* RGB screen res */
#define RGB_SC_WIDTH 1024
#define RGB_SC_HEIGHT 600

#define MAX_ENC_NUM 4

struct venc_pair
{
        int venc_handle;
        char *file; //rtsp client handle
        char save_file[128];
        int en_type;
};

/* resolation define */
struct resolution_t
{
        unsigned int width;
        unsigned int height;
        unsigned char str[20];
};

static char *pc_prog_name = NULL; //demo名称
static char *file = NULL;         //file name
static char *type = "h264";       //get the type input
static char *save_path = "/mnt/rtp_push_h264/";
static int main_res = 0;
static int sub_res = 0;
static int frame_num = 1;
static char *cfg = "/etc/jffs2/isp_ar0230_dvp.conf"; //"/etc/jffs2/isp_pr2000_dvp.conf";///etc/jffs2/isp_ar0230_dvp.conf
ak_mutex_t refresh_flag_lock;
static int chn_index = 0;

/* decoder resolution */
static struct resolution_t resolutions[DE_VIDEO_SIZE_MAX] = {
    {640, 360, "DE_VIDEO_SIZE_360P"},
    {640, 480, "DE_VIDEO_SIZE_VGA"},
    {1280, 720, "DE_VIDEO_SIZE_720P"},
    {960, 1080, "DE_VIDEO_SIZE_960"},
    {1920, 1080, "DE_VIDEO_SIZE_1080P"},
    {2560, 1920, "DE_VIDEO_SIZE_1920P"}};

/* this is the message to print */
// char ac_option_hint[][LEN_HINT] = {
//     //操作提示数组
//     "打印帮助信息",
//     "从vi获取的帧的数量",
//     "编码输出数据格式 h264 h265 jpeg ",
//     "主通道分辨率，0-5",
//     "次通道分辨率，0-3 need smaller than main channel",
//     "编码数据保存成文件的目录 ",
//     "isp config file",
//     "vi channel index, 0-main, 1-sub"
//     "",
// };

// /* opt for print the message */
// struct option option_long[] = {
//     {"help", no_argument, NULL, 'h'},               //"打印帮助信息" ,
//     {"frame-num", required_argument, NULL, 'n'},    //"屏幕类型" ,
//     {"data-output", required_argument, NULL, 'o'},  //"数据格式" ,
//     {"main_res  ", required_argument, NULL, 'm'},   //"分辨率" ,
//     {"sub_res  ", required_argument, NULL, 's'},    //"分辨率" ,
//     {"save-path", required_argument, NULL, 'p'},    //"解码数量" ,
//     {"isp-cfg-file", required_argument, NULL, 'c'}, //"解码数量" ,
//     {"channel", required_argument, NULL, 'a'},      //"解码数量" ,
//     {0, 0, 0, 0},
// };

/*
* get_option_short: fill the stort option string.
* return: option short string addr.
*/
// char *get_option_short(struct option *p_option, int i_num_option, char *pc_option_short, int i_len_option)
// {
//         int i;
//         int i_offset = 0;
//         char c_option;

//         /* get the option */
//         for (i = 0; i < i_num_option; i++)
//         {
//                 if ((c_option = p_option[i].val) == 0)
//                 {
//                         continue;
//                 }

//                 switch (p_option[i].has_arg)
//                 {
//                 case no_argument:
//                         /* if no argument, set the offset for default */
//                         i_offset += snprintf(pc_option_short + i_offset, i_len_option - i_offset, "%c", c_option);
//                         break;
//                 case required_argument:
//                         /* required argument offset calculate */
//                         i_offset += snprintf(pc_option_short + i_offset, i_len_option - i_offset, "%c:", c_option);
//                         break;
//                 case optional_argument:
//                         /* calculate the option offset */
//                         i_offset += snprintf(pc_option_short + i_offset, i_len_option - i_offset, "%c::", c_option);
//                         break;
//                 }
//         }
//         return pc_option_short;
// }

// void usage(const char *name)
// {
//         ak_print_normal(MODULE_ID_VDEC, "eg.: %s -n 1000 -o h264 -r 3 -p /mnt/video -a 0\n", name);
//         ak_print_normal(MODULE_ID_VDEC, "encode data type -o:          h264,  h265,  or  jpeg\n");
//         ak_print_normal(MODULE_ID_VDEC, "resolution -r:  value 0 ~ 4\n");
//         ak_print_normal(MODULE_ID_VDEC, "                      0 - 640*360\n");
//         ak_print_normal(MODULE_ID_VDEC, "                      1 - 640*480\n");
//         ak_print_normal(MODULE_ID_VDEC, "                      2 - 1280*720\n");
//         ak_print_normal(MODULE_ID_VDEC, "                      3 - 960*1280\n");
//         ak_print_normal(MODULE_ID_VDEC, "                      4 - 1920*1080\n");
//         ak_print_normal(MODULE_ID_VDEC, "                      5 - 2560*1920\n");
// }

// /* if opt is not supported, print the help message */
// static int help_hint(void)
// {
//         int i;

//         ak_print_normal(MODULE_ID_VDEC, "%s\n", pc_prog_name);
//         /* parse the all supported option */
//         for (i = 0; i < sizeof(option_long) / sizeof(struct option); i++)
//         {
//                 if (option_long[i].val != 0)
//                         ak_print_normal(MODULE_ID_VDEC, "\t--%-16s -%c %s\n",
//                                         option_long[i].name, option_long[i].val, ac_option_hint[i]);
//         }
//         usage(pc_prog_name);
//         ak_print_normal(MODULE_ID_VDEC, "\n\n");
//         return AK_SUCCESS;
// }

// /* parse the option from cmd line */
// int parse_option(int argc, char **argv)
// {
//         int i_option;

//         char ac_option_short[LEN_OPTION_SHORT];
//         int i_array_num = sizeof(option_long) / sizeof(struct option); /* get the option num*/
//         char c_flag = AK_TRUE;
//         pc_prog_name = argv[0]; /* get the option */

//         get_option_short(option_long, i_array_num, ac_option_short, LEN_OPTION_SHORT); /* parse the cmd line input */
//         while ((i_option = getopt_long(argc, argv, ac_option_short, option_long, NULL)) > 0)
//         {
//                 switch (i_option)
//                 {
//                 case 'h': //help
//                         help_hint();
//                         c_flag = AK_FALSE;
//                         goto parse_option_end;
//                 case 'n': //data file path
//                         frame_num = atoi(optarg);
//                         break;
//                 case 'o': //data_file format
//                         type = optarg;
//                         break;
//                 case 'm': //decode num
//                         main_res = atoi(optarg);
//                         break;
//                 case 's': //decode num
//                         sub_res = atoi(optarg);
//                         break;
//                 case 'p': //decode num
//                         save_path = optarg;
//                         break;
//                 case 'c': //decode num
//                         cfg = optarg;
//                         break;
//                 case 'a': //decode num
//                         chn_index = atoi(optarg);
//                         break;
//                 default:
//                         help_hint();
//                         c_flag = AK_FALSE;
//                         goto parse_option_end;
//                 }
//         }
// parse_option_end:
//         return c_flag;
// }

/* get the encode data from file path */
void *video_encode_from_vi_th(void *arg)
{
        ak_thread_set_name("venc_th");
        struct venc_pair *venc_th = (struct venc_pair *)arg; //venc handle

        /* a dir opt */
        int count = 0;
        FILE *save_fp = NULL;
        struct video_input_frame frame;

        ak_print_normal(MODULE_ID_VI, "capture start\n");

        /* init the save file name and open it */
        memset(venc_th->save_file, 0x00, sizeof(venc_th->save_file));
        if (H264_ENC_TYPE == venc_th->en_type)
                sprintf(venc_th->save_file, "%s/h264_chn%d_%d.str", save_path, chn_index, venc_th->venc_handle);
        else
                sprintf(venc_th->save_file, "%s/h265_chn%d_%d.str", save_path, chn_index, venc_th->venc_handle);

        /* save file open */
        if (MJPEG_ENC_TYPE != venc_th->en_type)
                save_fp = fopen(venc_th->save_file, "w+");

        /* frame num cal */
        while (1) //count < frame_num
        {
                memset(&frame, 0x00, sizeof(frame));

                /* if save as jpeg data */
                if (MJPEG_ENC_TYPE == venc_th->en_type)
                {
                        memset(venc_th->save_file, 0x00, sizeof(venc_th->save_file));
                        sprintf(venc_th->save_file, "%s/chn%d_%d_num_%d.jpeg", save_path, chn_index, venc_th->venc_handle, count);
                        save_fp = fopen(venc_th->save_file, "w+");
                }

                /* to get frame */
                VI_CHN chn_id = (chn_index == 0) ? VIDEO_CHN0 : VIDEO_CHN1;
                int ret = ak_vi_get_frame(chn_id, &frame);
                if (!ret)
                {
                        /* send it to encode */
                        struct video_stream *stream = ak_mem_alloc(MODULE_ID_VENC, sizeof(struct video_stream));
                        ret = ak_venc_encode_frame(venc_th->venc_handle, frame.vi_frame.data, frame.vi_frame.len, frame.mdinfo, stream);
                        if (ret)
                        {
                                /* send to encode failed */
                                printf("ERROR: send to encode failed\n");
                        }
                        else
                        {
                                fwrite(stream->data, stream->len, 1, save_fp);
                                ak_venc_release_stream(venc_th->venc_handle, stream);
                                // count++;
                                printf("h264 update\n");
                                rtp();
                                printf("rtp send\n");
                        }

                        ak_mem_free(stream);
                        ak_vi_release_frame(chn_id, &frame);
                }
                else
                {
                        //If getting too fast, it will have no data,just take breath.
                        printf("get frame too fast!\n");
                        ak_sleep_ms(10);
                }

                /* save as a jpeg picture */
                if (MJPEG_ENC_TYPE == venc_th->en_type)
                {
                        if (NULL != save_fp)
                        {
                                fclose(save_fp);
                                save_fp = NULL;
                        }
                }
        }

        /* finished */
        if (MJPEG_ENC_TYPE != venc_th->en_type)
                fclose(save_fp);

        return NULL;
}

/**
 * note: read the appointed video file, decode, and then output to vo  to playing
 */
int venc_rtp() //int argc, char **argv
{
        /* init sdk running */
        sdk_run_config config;
        config.mem_trace_flag = SDK_RUN_DEBUG;
        ak_sdk_init(&config);

        /* start to parse the opt */
        // if (parse_option(argc, argv) == AK_FALSE)
        // { //解释和配置选项
        //         return AK_FAILED;
        // }

        /* get the type for input */
        int encoder_type = 0;
        if (!strcmp(type, "h264"))
        {
                /* h264 */
                ak_print_normal(MODULE_ID_VDEC, "h264 success\n");
                encoder_type = H264_ENC_TYPE;
        }
        else if (!strcmp(type, "h265"))
        {
                /* h265 */
                ak_print_normal(MODULE_ID_VDEC, "h265 success\n");
                encoder_type = HEVC_ENC_TYPE;
        }
        else if (!strcmp(type, "jpeg"))
        {
                /* jpeg */
                ak_print_normal(MODULE_ID_VDEC, "jpeg success\n");
                encoder_type = MJPEG_ENC_TYPE;
        }
        else
        {
                ak_print_normal_ex(MODULE_ID_VDEC, "unsupport video enccode input type [%s] \n", type);
                return AK_FAILED;
        }

        /* get param from cmd line */
        int ret = -1;
        int width = resolutions[main_res].width;
        int height = resolutions[main_res].height;
        int subwidth = resolutions[sub_res].width;
        ;
        int subheight = resolutions[sub_res].height;
        int handle_id = -1;
        struct venc_pair enc_pair;
        ak_pthread_t venc_stream_th;

        /* step 1: open video input device */
        ret = ak_vi_open(VIDEO_DEV0);
        if (AK_SUCCESS != ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device open failed\n");
                return AK_FAILED;
        }

        /*
     * step 2: load isp config
     */
        ret = ak_vi_load_sensor_cfg(VIDEO_DEV0, cfg);
        if (AK_SUCCESS != ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device load cfg [%s] failed!\n", cfg);
                return AK_FAILED;
        }

        /* 
     * step 3: get sensor support max resolution
     */
        RECTANGLE_S venc_res; //max sensor resolution
        VI_DEV_ATTR dev_attr;
        dev_attr.dev_id = VIDEO_DEV0;
        dev_attr.crop.left = 0;
        dev_attr.crop.top = 0;
        dev_attr.crop.width = width;
        dev_attr.crop.height = height;
        dev_attr.max_width = width;
        dev_attr.max_height = height;
        dev_attr.sub_max_width = subwidth;
        dev_attr.sub_max_height = subheight;

        ret = ak_vi_get_sensor_resolution(VIDEO_DEV0, &venc_res);
        if (ret)
        {
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }
        else
        {
                ak_print_normal_ex(MODULE_ID_VI, "get dev res w:[%d]h:[%d]\n", venc_res.width, venc_res.height);
                dev_attr.crop.width = venc_res.width;
                dev_attr.crop.height = venc_res.height;
        }

        /* 
     * step 4: set vi working parameters 
     * default parameters: 25fps, day mode, auto frame-control
     */
        ret = ak_vi_set_dev_attr(VIDEO_DEV0, &dev_attr);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device set device attribute failed!\n");
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }

        /*
     * step 5: set channel attribute
     */
        VI_CHN_ATTR chn_attr;
        chn_attr.chn_id = VIDEO_CHN0;
        chn_attr.res.width = width;
        chn_attr.res.height = height;
        chn_attr.frame_depth = 3;
        chn_attr.frame_rate = 0;
        ret = ak_vi_set_chn_attr(VIDEO_CHN0, &chn_attr);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device set channel attribute failed!\n");
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }

        /*
     * step 5: set sub channel attribute
     */
        VI_CHN_ATTR chn_attr_sub;
        chn_attr_sub.chn_id = VIDEO_CHN1;
        chn_attr_sub.res.width = subwidth;
        chn_attr_sub.res.height = subheight;
        chn_attr_sub.frame_depth = 3;
        chn_attr_sub.frame_rate = 0;
        ret = ak_vi_set_chn_attr(VIDEO_CHN1, &chn_attr_sub);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device set channel [%d] attribute failed!\n", VIDEO_CHN1);
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }
        ak_print_normal_ex(MODULE_ID_VI, "vi device set sub channel attribute\n");

        /* 
     * step 6: start capture frames
     */
        ret = ak_vi_enable_dev(VIDEO_DEV0);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device enable device  failed!\n");
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }
        /* 
     * step 7: enable vi main channel 
     */
        ret = ak_vi_enable_chn(VIDEO_CHN0);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi channel[%d] enable failed!\n", VIDEO_CHN0);
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }

        /* 
     * step 7: enable vi sub channel 
     */
        ret = ak_vi_enable_chn(VIDEO_CHN1);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi channel[%d] enable failed!\n", VIDEO_CHN1);
                ak_vi_close(VIDEO_DEV0);
                return AK_FAILED;
        }

        if (access(save_path, W_OK) != 0)
        {
                if (mkdir(save_path, 0755))
                {
                        ak_print_error_ex(MODULE_ID_VENC, "mkdir: %s failed\n", save_path);
                        return AK_FAILED;
                }
        }

        /* open venc */
        struct venc_param ve_param;

        if (chn_index == 0)
        {
                ve_param.width = width;   //resolution width
                ve_param.height = height; //resolution height
        }
        else
        {
                ve_param.width = subwidth;   //resolution width
                ve_param.height = subheight; //resolution height
        }

        ve_param.fps = 20;                                       //fps set
        ve_param.goplen = 50;                                    //gop set
        ve_param.target_kbps = 800;                              //k bps
        ve_param.max_kbps = 1024;                                //max kbps
        ve_param.br_mode = BR_MODE_CBR;                          //br mode
        ve_param.minqp = 25;                                     //qp set
        ve_param.maxqp = 50;                                     //qp max value
        ve_param.initqp = (ve_param.minqp + ve_param.maxqp) / 2; //qp value
        ve_param.jpeg_qlevel = JPEG_QLEVEL_DEFAULT;              //jpeg qlevel
        ve_param.chroma_mode = CHROMA_4_2_0;                     //chroma mode
        ve_param.max_picture_size = 0;                           //0 means default
        ve_param.enc_level = 30;                                 //enc level
        ve_param.smart_mode = 0;                                 //smart mode set
        ve_param.smart_goplen = 100;                             //smart mode value
        ve_param.smart_quality = 50;                             //quality
        ve_param.smart_static_value = 0;                         //value
        ve_param.enc_out_type = encoder_type;                    //enc type

        /* decode type is h264*/
        if (encoder_type == H264_ENC_TYPE)
        {
                /* profile type */
                ve_param.profile = PROFILE_MAIN;
        }
        /*decode type is jpeg*/
        else if (encoder_type == MJPEG_ENC_TYPE)
        {
                /* jpeg enc profile */
                ve_param.initqp = 80;
                ve_param.profile = PROFILE_JPEG;
        }
        /*decode type is h265*/
        else
        {
                /* hevc profile */
                ve_param.profile = PROFILE_HEVC_MAIN;
        }

        ret = ak_venc_open(&ve_param, &handle_id);

        if (ret || (-1 == handle_id))
        {
                /* close the video encoder and video decoder */
                if (enc_pair.venc_handle != -1)
                {
                        /* close the venc*/
                        ak_venc_close(enc_pair.venc_handle);
                }

                printf("error\n");

                ak_vi_disable_chn(VIDEO_CHN0);
                ak_vi_disable_chn(VIDEO_CHN1);
                ak_vi_disable_dev(VIDEO_DEV0);
                ak_vi_close(VIDEO_DEV0);

                return ret;
        }

        /* recode the file */
        enc_pair.file = file;
        enc_pair.venc_handle = handle_id;
        enc_pair.en_type = encoder_type;

        /* create the venc thread */
        ak_thread_create(&venc_stream_th, video_encode_from_vi_th, &enc_pair, ANYKA_THREAD_MIN_STACK_SIZE, THREAD_PRIO);

        /* WAITER for the thread exit */
        ak_thread_join(venc_stream_th);
}

#ifdef AK_RTOS
MSH_CMD_EXPORT(vdec_sample_main, Video Decode Sample Program);
#endif

/* end of file */
