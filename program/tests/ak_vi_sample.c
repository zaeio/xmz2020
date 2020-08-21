/**
* Copyright (C) 2018 Anyka(Guangzhou) Microelectronics Technology CO.,LTD.
* File Name: ak_vi_demo.c
* Description: This is a simple example to show how the VI module working.
* Notes:
* History: V1.0.0
*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "ak_common.h"
#include "ak_common_graphics.h"
// #include "ak_vo.h"
#include "ak_vi.h"
#include "ak_mem.h"
#include "ak_log.h"
// #include "ak_tde.h"

#include "fbutils.h"

#define LEN_HINT 512
#define LEN_OPTION_SHORT 512
#define DEF_FRAME_DEPTH 3
#define RES_GROUP 7

VI_CHN_ATTR chn_attr;     //main channel attribute
VI_CHN_ATTR chn_attr_sub; //sub channel attribute

int frame_num = 0; //=0时连续拍摄
int channel_num = 0;
char *isp_path = "/etc/jffs2/isp_ar0230_dvp.conf";
// char *save_path = "/mnt/frame/";
int main_res_id = 6; //used resolution
int sub_res_id = 0;
int vi_capture_enable = 0;

/* support resolution list */
int res_group[RES_GROUP][2] = {
    {640, 360},   /* 640*360 */
    {640, 480},   /*   VGA   */
    {1280, 720},  /*   720P  */
    {960, 1080},  /*   1080i */
    {1920, 1080}, /*   1080P */
    {608, 600},
    {896, 600}};

struct show_frame_param
{
        unsigned char *yuvdata;
        int width;
        int height;
};

/* 
 * check_dir: check whether the 'path' was exist.
 * path[IN]: pointer to the path which will be checking.
 * return: 1 on exist, 0 is not.
 */
static int check_dir(const char *path)
{
        struct stat buf = {0};

        if (NULL == path)
                return 0;

        stat(path, &buf);
        if (S_ISDIR(buf.st_mode))
        {
                return 1;
        }
        else
        {
                return 0;
        }
}

/* 
 * save_yuv_data - use to save yuv data to file
 * path[IN]: pointer to saving directory
 * index[IN]: frame number index
 * frame[IN]: pointer to yuv data, include main and sub channel data
 * attr[IN]:  vi channel attribute.
 */
static void save_yuv_data(const char *path, int index, struct video_input_frame *frame, VI_CHN_ATTR *attr)
{
        FILE *fd = NULL;
        unsigned int len = 0;
        unsigned char *buf = NULL;
        struct ak_date date;
        char time_str[32] = {0};
        char file_path[255] = {0};

        ak_print_normal(MODULE_ID_VI, "saving frame, index=%d\n", index);

        /* construct file name */
        ak_get_localdate(&date);
        ak_date_to_string(&date, time_str);
        if (channel_num == VIDEO_CHN0)
                sprintf(file_path, "%smain_%s_%d_%dx%d.yuv", path, time_str, index,
                        attr->res.width, attr->res.height);
        else
                sprintf(file_path, "%ssub_%s_%d_%dx%d.yuv", path, time_str, index,
                        attr->res.width, attr->res.height);

        /* 
	 * open appointed file to save YUV data
	 * save main channel yuv here
	 */
        fd = fopen(file_path, "w+b");
        if (fd)
        {
                buf = frame->vi_frame.data;
                len = frame->vi_frame.len;
                do
                {
                        len -= fwrite(buf, 1, len, fd);
                } while (len != 0);

                fclose(fd);
        }
        else
        {
                ak_print_normal(MODULE_ID_VI, "open YUV file failed!!\n");
        }
}

void show_frame(void *arg)
{
        uint32_t *RGBmap;
        struct show_frame_param *param;
        param = (struct show_frame_para *)arg;

        RGBmap = yuv420_to_rgb(param->yuvdata, param->width, param->height);
        put_rgb_map(0, 0, RGBmap, param->width, param->height);
}

/*
 * vi_capture_loop: loop to get and release yuv, between get and release,
 *                  here we just save the frame to file, on your platform,
 *                  you can rewrite the save_function with your code.
 * vi_handle[IN]: pointer to vi handle, return by ak_vi_open()
 * number[IN]: save numbers
 * path[IN]:   save directory path, if NULL, will not save anymore.
 * attr[IN]:   vi channel attribute.
 */

/*static void vi_capture_loop(VI_DEV dev_id, int number, VI_CHN_ATTR *attr, VI_CHN_ATTR *attr_sub)
{
        int count = 0;
        struct video_input_frame frame;
        uint32_t *RGBmap;
        // int thread_read_flag = 0;
        // pthread_t thID_showframe;
        // struct show_frame_param param;

        ak_print_normal(MODULE_ID_VI, "capture start\n");

	//To get frame by loop
	 
        while (count < number || number == 0)
        {
                memset(&frame, 0x00, sizeof(frame));

                //to get frame according to the channel number 
                int ret = ak_vi_get_frame(channel_num, &frame);

                if (!ret)
                {
                        
			 //Here, you can implement your code to use this frame.Notice, do not occupy it too long.
			 
                        // param.yuvdata = frame.vi_frame.data;
                        // param.width = res_group[main_res_id][0];
                        // param.height = res_group[main_res_id][1];
                        // if (pthread_create(&thID_showframe, NULL, (void *)show_frame, &param) != 0)
                        // {
                        //         printf("Create pthread error!\n");
                        //         exit(1);
                        // }

                        RGBmap = yuv420_to_rgb(frame.vi_frame.data, res_group[main_res_id][0], res_group[main_res_id][1]);
                        put_rgb_map(0, 0, RGBmap, res_group[main_res_id][0], res_group[main_res_id][1]);
                        free(RGBmap);

                        // if (channel_num == VIDEO_CHN0)
                        //         save_yuv_data(save_path, count, &frame, attr);
                        // else
                        //         save_yuv_data(save_path, count, &frame, attr_sub);

                        ak_vi_release_frame(channel_num, &frame);
                        count++;
                }
                else
                {
                        
			 //If getting too fast, it will have no data,just take breath.
			 
                        printf("get frmae failed! sleep 10ms\n");
                        ak_sleep_ms(10);
                }
        }

        ak_print_normal(MODULE_ID_VI, "capture finish\n\n");
}*/

void vi_capture_loop()
{
        struct video_input_frame frame;
        // uint32_t *RGBmap;
        char *graymap;

        printf("*** vi capture start ***\n");

        while (vi_capture_enable)
        {
                memset(&frame, 0x00, sizeof(frame));

                //to get frame according to the channel number
                int ret = ak_vi_get_frame(channel_num, &frame);

                if (!ret)
                {
                        // RGBmap = yuv420_to_rgb(frame.vi_frame.data, res_group[main_res_id][0], res_group[main_res_id][1]);
                        // put_rgb_map(0, 0, RGBmap, res_group[main_res_id][0], res_group[main_res_id][1]);

                        graymap = yuv420_to_gray(frame.vi_frame.data, res_group[main_res_id][0], res_group[main_res_id][1]);
                        put_gray_map(0, 0, graymap, res_group[main_res_id][0], res_group[main_res_id][1]);
                        // send_Vframe(graymap, res_group[main_res_id][0] * res_group[main_res_id][1]);

                        // free(RGBmap);
                        free(graymap);
                        ak_vi_release_frame(channel_num, &frame);
                }
                else
                {
                        printf("get frmae failed! sleep 10ms\n");
                        ak_sleep_ms(10);
                }
        }
        printf("*** vi capture finish ***\n");
        pthread_exit(NULL);
}

/**
 * Preconditions:
 * 1??TF card is already mounted
 * 2??yuv_data is already created in /mnt
 * 3??ircut is already opened at day mode
 * 4??your main video progress must stop
 */
//int main(int argc, char **argv)
int ak_vi_init()
{
        /* start the application */
        sdk_run_config config;
        config.mem_trace_flag = SDK_RUN_DEBUG;
        ak_sdk_init(&config);

        /*check the data save path */
        // if (check_dir(save_path) == 0)
        // {
        //         ak_print_error_ex(MODULE_ID_VI, "save path is not existed!\n");
        //         return 0;
        // }

        /* 
	 * step 0: global value initialize
	 */
        int ret = -1; //return value
        int width = res_group[main_res_id][0];
        int height = res_group[main_res_id][1];
        int subwidth = res_group[sub_res_id][0];
        int subheight = res_group[sub_res_id][1];

        /* open vi flow */

        /* 
	 * step 1: open video input device
	 */
        ret = ak_vi_open(VIDEO_DEV0);
        if (AK_SUCCESS != ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device open failed\n");
                goto exit;
        }

        /*
	 * step 2: load isp config
	 */
        ret = ak_vi_load_sensor_cfg(VIDEO_DEV0, isp_path);
        if (AK_SUCCESS != ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device load isp cfg [%s] failed!\n", isp_path);
                goto exit;
        }

        /* 
	 * step 3: get sensor support max resolution
	 */
        RECTANGLE_S res; //max sensor resolution
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

        /* get sensor resolution */
        ret = ak_vi_get_sensor_resolution(VIDEO_DEV0, &res);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "Can't get dev[%d]resolution\n", VIDEO_DEV0);
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }
        else
        {
                ak_print_normal_ex(MODULE_ID_VI, "get dev res w:[%d]h:[%d]\n", res.width, res.height);
                dev_attr.crop.width = res.width;
                dev_attr.crop.height = res.height;
        }

        /* 
	 * step 4: set vi device working parameters 
	 * default parameters: 25fps, day mode
	 */
        ret = ak_vi_set_dev_attr(VIDEO_DEV0, &dev_attr);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device set device attribute failed!\n");
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }

        /*
	 * step 5: set main channel attribute
	 */
        chn_attr.chn_id = VIDEO_CHN0;
        chn_attr.res.width = width;
        chn_attr.res.height = height;
        chn_attr.frame_depth = DEF_FRAME_DEPTH;
        /*disable frame control*/
        chn_attr.frame_rate = 0;
        ret = ak_vi_set_chn_attr(VIDEO_CHN0, &chn_attr);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device set channel [%d] attribute failed!\n", VIDEO_CHN0);
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }
        ak_print_normal_ex(MODULE_ID_VI, "vi device main sub channel attribute\n");

        /*
	 * step 6: set sub channel attribute
	 */

        chn_attr_sub.chn_id = VIDEO_CHN1;
        chn_attr_sub.res.width = subwidth;
        chn_attr_sub.res.height = subheight;
        chn_attr_sub.frame_depth = DEF_FRAME_DEPTH;
        /*disable frame control*/
        chn_attr_sub.frame_rate = 0;
        ret = ak_vi_set_chn_attr(VIDEO_CHN1, &chn_attr_sub);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device set channel [%d] attribute failed!\n", VIDEO_CHN1);
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }
        ak_print_normal_ex(MODULE_ID_VI, "vi device set sub channel attribute\n");

        /* 
	 * step 7: enable vi device 
	 */
        ret = ak_vi_enable_dev(VIDEO_DEV0);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi device enable device  failed!\n");
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }

        /* 
	 * step 8: enable vi main channel 
	 */
        ret = ak_vi_enable_chn(VIDEO_CHN0);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi channel[%d] enable failed!\n", VIDEO_CHN0);
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }

        /* 
	 * step 9: enable vi sub channel 
	 */
        ret = ak_vi_enable_chn(VIDEO_CHN1);
        if (ret)
        {
                ak_print_error_ex(MODULE_ID_VI, "vi channel[%d] enable failed!\n", VIDEO_CHN1);
                ak_vi_close(VIDEO_DEV0);
                goto exit;
        }

        /* 
	 * step 10: start to capture and save yuv frames 
	 */
        // vi_capture_loop(VIDEO_DEV0, frame_num, &chn_attr, &chn_attr_sub);

        /*
	 * step 11: release resource
	 */
        // ak_vi_disable_chn(VIDEO_CHN0);
        // ak_vi_disable_chn(VIDEO_CHN1);
        // ak_vi_disable_dev(VIDEO_DEV0);
        // ret = ak_vi_close(VIDEO_DEV0);

exit:
        /* exit */
        ak_print_normal(MODULE_ID_VI, "exit vi demo\n");
        return ret;
}

void ak_vi_disable()
{
        ak_vi_disable_chn(VIDEO_CHN0);
        ak_vi_disable_chn(VIDEO_CHN1);
        ak_vi_disable_dev(VIDEO_DEV0);
        ak_vi_close(VIDEO_DEV0);
}
