/**
* Copyright (C) 2018 Anyka(Guangzhou) Microelectronics Technology CO.,LTD.
* File Name: ak_ai_demo.c
* Description: This is a simple example to show how the AI module working.
* Notes:
* History: V1.0.1
*/
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <getopt.h>

#include "ak_ai.h"
#include "ak_common.h"
#include "ak_log.h"
#include "ak_mem.h"

#define LEN_HINT 512
#define LEN_OPTION_SHORT 512

FILE *fp_pcm_i = NULL;
int sample_rate = 8000; //8000 12000 11025 16000 22050 24000 32000 44100 48000
int volume = 6;
int save_time = 20000;              // set save time(ms)
char *ai_save_path = "/mnt/frame/"; // set save path
int ai_capture_enable = 0;          //按下按键录音
int ai_handle_id = -1;
// int channel_num = AUDIO_CHANNEL_MONO;//=1

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
 * get_frame_len: get frame length
 * encode_type[IN]: encode type
 * sample_rate[IN]: sample rate
 * channel[IN]: channel number
 * return: 
 */
static int get_pcm_frame_len(int sample_rate, int channel)
{
        int frame_len = 960;

        switch (sample_rate)
        {
        case AK_AUDIO_SAMPLE_RATE_8000:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 960 : 1600;
                break;
        case AK_AUDIO_SAMPLE_RATE_11025:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 882 : 1764;
                break;
        case AK_AUDIO_SAMPLE_RATE_12000:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 960 : 1920;
                break;
        case AK_AUDIO_SAMPLE_RATE_16000:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 1280 : 2560;
                break;
        case AK_AUDIO_SAMPLE_RATE_22050:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 1764 : 3528;
                break;
        case AK_AUDIO_SAMPLE_RATE_24000:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 1920 : 2840;
                break;
        case AK_AUDIO_SAMPLE_RATE_32000:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 2560 : 5120;
                break;
        case AK_AUDIO_SAMPLE_RATE_44100:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 3528 : 5292;
                break;
        case AK_AUDIO_SAMPLE_RATE_48000:
                frame_len = (channel == AUDIO_CHANNEL_MONO) ? 4800 : 5760;
                break;

        default:
                break;
        }

        ak_print_notice_ex(MODULE_ID_AENC, "frame_len=%d\n", frame_len);

        return frame_len;
}

/*
 * create_pcm_file_name: create pcm file by path+date.
 * path[IN]: pointer to the path which will be checking.
 * file_path[OUT]: pointer to the full path of pcm file.
 * return: void.
 */
static void create_pcm_file_name(const char *path, char *file_path, int sample_rate, int channel_num)
{
        if (0 == check_dir(path))
        {
                return;
        }

        // char time_str[20] = {0};
        // struct ak_date date;

        /* get the file path */
        // ak_get_localdate(&date);
        // ak_date_to_string(&date, time_str);
        // sprintf(file_path, "%s%s_%d_%d.pcm", path, time_str, sample_rate, channel_num);
        sprintf(file_path, "%sak_ao_test.pcm", path);
}

/*
 * open_pcm_file: open pcm file.
 * path[IN]: pointer to the path which will be checking.
 * fp_pcm_i[OUT]: pointer of opened pcm file.
 * return: void.
 */
static void open_pcm_file(const char *path)
{
        /* create the pcm file name */
        char file_path[255];
        if (0 == check_dir(path))
        {
                return;
        }
        sprintf(file_path, "%sak_ao_test.pcm", path);

        /* open file */
        fp_pcm_i = fopen(file_path, "w+b");
        if (NULL == fp_pcm_i)
        {
                printf("open pcm file: %s error\n", file_path);
        }
}

static void close_pcm_file(FILE *fp_pcm_i)
{
        if (NULL != fp_pcm_i)
        {
                fclose(fp_pcm_i);
                fp_pcm_i = NULL;
        }
}

/*
 * ai_capture_loop: loop to get and release pcm data, between get and release,
 *                  here we just save the frame to file, on your platform,
 *                  you can rewrite the save_function with your code.
 * ai_handle_id[IN]: pointer to ai handle, return by ak_ai_open()
 * path[IN]: save directory path, if NULL, will not save anymore.
 * save_time[IN]: captured time of pcm data, unit is second.
 */
void ai_capture_loop()
{
        unsigned long long start_ts = 0; // use to save capture start time
        unsigned long long end_ts = 0;   // the last frame time
        struct frame frame = {0};
        int ret = AK_FAILED;
        // int ai_handle_id = *(int *)arg;

        open_pcm_file(ai_save_path);
        if (fp_pcm_i == NULL)
        {
                printf("ERROR: pcm file is NULL\n");
                return;
        }

        printf("*** capture start ***\n");
        while (ai_capture_enable)
        {
                /* get the pcm data frame */
                ret = ak_ai_get_frame(ai_handle_id, &frame, 0);
                if (ret)
                {
                        if (ERROR_AI_NO_DATA == ret)
                        {
                                ak_sleep_ms(10);
                                continue;
                        }
                        else
                        {
                                break;
                        }
                }

                if (!frame.data || frame.len <= 0)
                {
                        ak_sleep_ms(10);
                        continue;
                }
                if (fwrite(frame.data, frame.len, 1, fp_pcm_i) < 0)
                {
                        ak_ai_release_frame(ai_handle_id, &frame);
                        printf("write file error.\n");
                        break;
                }

                ak_ai_release_frame(ai_handle_id, &frame);
        }
        // ak_ai_close(ai_handle_id);
        fclose(fp_pcm_i);
        printf("*** capture finish ***\n");
}

int ak_ai_init()
{
        /* start the application */
        sdk_run_config config;
        config.mem_trace_flag = SDK_RUN_DEBUG;
        int channel_num = 1;
        ak_sdk_init(&config);

        int ret = -1;

        struct ak_audio_in_param param;
        memset(&param, 0, sizeof(struct ak_audio_in_param));
        ak_print_notice_ex(MODULE_ID_AI, "sample_rate=%d\n", sample_rate);
        param.pcm_data_attr.sample_rate = sample_rate;           // set sample rate
        param.pcm_data_attr.sample_bits = AK_AUDIO_SMPLE_BIT_16; // sample bits only support 16 bit
        param.pcm_data_attr.channel_num = channel_num;           // channel number
        param.dev_id = 0;

        // int ai_handle_id = -1;
        if (ak_ai_open(&param, &ai_handle_id))
        {
                ak_print_normal(MODULE_ID_AI, "*** ak_ai_open failed. ***\n");
                goto exit;
        }

        int frame_len = get_pcm_frame_len(sample_rate, channel_num);
        ret = ak_ai_set_frame_length(ai_handle_id, frame_len);
        if (ret)
        {
                ak_print_normal(MODULE_ID_AI, "*** set ak_ai_set_frame_interval failed. ***\n");
                ak_ai_close(ai_handle_id);
                goto exit;
        }

        /* set source, source include mic and linein */
        ret = ak_ai_set_source(ai_handle_id, AI_SOURCE_MIC);
        if (ret)
        {
                ak_print_normal(MODULE_ID_AI, "*** set ak_ai_open failed. ***\n");
                ak_ai_close(ai_handle_id);
                goto exit;
        }

        ret = ak_ai_set_gain(ai_handle_id, 4);
        if (ret)
        {
                ak_print_normal(MODULE_ID_AI, "*** set ak_ai_set_volume failed. ***\n");
                ak_ai_close(ai_handle_id);
                goto exit;
        }

        ak_ai_set_volume(ai_handle_id, volume);

        ret = ak_ai_enable_nr(ai_handle_id, AUDIO_FUNC_ENABLE);
        if (ret)
        {
                ak_print_error(MODULE_ID_AI, "*** set ak_ai_set_nr_agc failed. ***\n");
        }

        ret = ak_ai_enable_agc(ai_handle_id, AUDIO_FUNC_ENABLE);
        if (ret)
        {
                ak_print_error(MODULE_ID_AI, "*** set ak_ai_set_nr_agc failed. ***\n");
        }

        ret = ak_ai_clear_frame_buffer(ai_handle_id);
        if (ret)
        {
                ak_print_error(MODULE_ID_AI, "*** set ak_ai_clear_frame_buffer failed. ***\n");
                ak_ai_close(ai_handle_id);
                goto exit;
        }

        ret = ak_ai_start_capture(ai_handle_id);
        if (ret)
        {
                ak_print_error(MODULE_ID_AI, "*** ak_ai_start_capture failed. ***\n");
                ak_ai_close(ai_handle_id);
                goto exit;
        }

        // ai_capture_loop(ai_handle_id, ai_save_path, save_time);

        // ret = ak_ai_stop_capture(ai_handle_id);
        // if (ret)
        // {
        //         ak_print_error(MODULE_ID_AI, "*** ak_ai_stop_capture failed. ***\n");
        //         ak_ai_close(ai_handle_id);
        //         goto exit;
        // }

        // ret = ak_ai_close(ai_handle_id);
        // if (ret)
        // {
        //         ak_print_normal(MODULE_ID_AI, "*** ak_ai_close failed. ***\n");
        // }

exit:
        /* close file handle */
        close_pcm_file(fp_pcm_i);
        ak_print_normal(MODULE_ID_AI, "******** exit ai demo ********\n");
        return ret;
}

void ak_ai_disable()
{
        if (ak_ai_stop_capture(ai_handle_id))
        {
                ak_print_error(MODULE_ID_AI, "*** ak_ai_stop_capture failed. ***\n");
                ak_ai_close(ai_handle_id);
        }

        if (ak_ai_close(ai_handle_id))
        {
                ak_print_normal(MODULE_ID_AI, "*** ak_ai_close failed. ***\n");
        }
}