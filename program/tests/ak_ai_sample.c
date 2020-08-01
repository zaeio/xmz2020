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

#define LEN_HINT         512
#define LEN_OPTION_SHORT 512

FILE *fp = NULL;
int sample_rate = 8000;
int volume = 5;
int save_time = 20000;        // set save time(ms)
char *save_path= "/mnt/";    // set save path
int channel_num = AUDIO_CHANNEL_MONO;

/*     ***********************************
    ***********************************
    *
    use this demo
    must follow this:
    1. make sure the driver is insmode;
    2. mount the T card;
    3. the file path is exit;
    *
    ***********************************
    ***********************************
*/

char ac_option_hint[  ][ LEN_HINT ] = {
    "       打印帮助信息" ,
    "[SEC]  采集时间" ,
    "[NUM]  采样率[8000 12000 11025 16000 22050 24000 32000 44100 48000]" ,
    "[NUM]    通道数[1 2]" ,
    "[NUM]  音量[-infinity~+infinity]" ,
    "[PATH] 文件保存路径" ,
};

struct option option_long[ ] = {
    { "help"        , no_argument       , NULL , 'h' } , //"       打印帮助信息" ,
    { "second"      , required_argument , NULL , 't' } , //"[SEC]  采集时间" ,
    { "sample-rate" , required_argument , NULL , 's' } , //"[NUM]  采样率[8000 16000 32000 44100 48000]" ,
    { "channel-number" , required_argument , NULL , 'c' } , //"[NUM]  通道数[1 2]" ,
    { "volume"      , required_argument , NULL , 'v' } , //"[NUM]  音量[0-12]" ,
    { "path"        , required_argument , NULL , 'p' } , //"[PATH] 文件保存路径" ,
    {0, 0, 0, 0}
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
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 960 :1600;
        break;
    case AK_AUDIO_SAMPLE_RATE_11025:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 882 :1764;
        break;
    case AK_AUDIO_SAMPLE_RATE_12000:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 960 :1920;
        break;
    case AK_AUDIO_SAMPLE_RATE_16000:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 1280 :2560;
        break;
    case AK_AUDIO_SAMPLE_RATE_22050:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 1764 :3528;
        break;
    case AK_AUDIO_SAMPLE_RATE_24000:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 1920 :2840;
        break;
    case AK_AUDIO_SAMPLE_RATE_32000:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 2560 :5120;
        break;
    case AK_AUDIO_SAMPLE_RATE_44100:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 3528 :5292;
        break;
    case AK_AUDIO_SAMPLE_RATE_48000:
        frame_len = (channel == AUDIO_CHANNEL_MONO) ? 4800 :5760;
        break;

    default:
        break;
    }

    ak_print_notice_ex(MODULE_ID_AENC, "frame_len=%d\n", frame_len);

    return frame_len;
}

static void print_playing_dot(void)
{
    static unsigned char first_flag = 1;
    static struct ak_timeval cur_time;
    static struct ak_timeval print_time;

    if (first_flag) 
    {
        first_flag = 0;
        ak_get_ostime(&cur_time);
        ak_get_ostime(&print_time);
        ak_print_normal(MODULE_ID_AI, "\n.");
    }

    /* get time */
    ak_get_ostime(&cur_time);
    if (ak_diff_ms_time(&cur_time, &print_time) >= 1000) 
    {
        ak_get_ostime(&print_time);
        ak_print_normal(MODULE_ID_AI, ".");
    }
}


/*
 * create_pcm_file_name: create pcm file by path+date.
 * path[IN]: pointer to the path which will be checking.
 * file_path[OUT]: pointer to the full path of pcm file.
 * return: void.
 */
static void create_pcm_file_name(const char *path, char *file_path,
                                        int sample_rate, int channel_num)
{
    if (0 == check_dir(path)) 
    {
        return;
    }

    char time_str[20] = {0};
    struct ak_date date;

    /* get the file path */
    ak_get_localdate(&date);
    ak_date_to_string(&date, time_str);
    sprintf(file_path, "%s%s_%d_%d.pcm", path, time_str, sample_rate, channel_num);
}

/*
 * open_pcm_file: open pcm file.
 * path[IN]: pointer to the path which will be checking.
 * fp[OUT]: pointer of opened pcm file.
 * return: void.
 */
static void open_pcm_file(int sample_rate, int channel_num, const char *path, FILE **fp)
{
    /* create the pcm file name */
    char file_path[255];
    create_pcm_file_name(path, file_path, sample_rate, channel_num);

    /* open file */
    *fp = fopen(file_path, "w+b");
    if (NULL == *fp) 
    {
        ak_print_normal(MODULE_ID_AI, "open pcm file: %s error\n", file_path);
    } 
    else 
    {
        ak_print_normal(MODULE_ID_AI, "open pcm file: %s OK\n", file_path);
    }
}

/*
 * close_pcm_file: close pcm file.
 * fp[IN]: pointer of opened pcm file.
 * return: void.
 */
static void close_pcm_file(FILE *fp)
{
    if (NULL != fp) 
    {
        fclose(fp);
        fp = NULL;
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
static void ai_capture_loop(int ai_handle_id, const char *path, int save_time)
{
    unsigned long long start_ts = 0;// use to save capture start time
    unsigned long long end_ts = 0;    // the last frame time
    struct frame frame = {0};
    int ret = AK_FAILED;

    ak_print_normal(MODULE_ID_AI, "*** capture start ***\n");

    while (1) 
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

        print_playing_dot();
        
        if (!frame.data || frame.len <= 0)
        {
            ak_sleep_ms(10);
            continue;
        }

        if (NULL != fp) 
        {
            if(fwrite(frame.data, frame.len, 1, fp) < 0) 
            {
                ak_ai_release_frame(ai_handle_id, &frame);
                ak_print_normal(MODULE_ID_AI, "write file error.\n");
                break;
            }
        }

        /* save the begin time */
        if (0 == start_ts) 
        {
            start_ts = frame.ts;
            end_ts = frame.ts;
        }
        end_ts = frame.ts;

        ak_ai_release_frame(ai_handle_id, &frame);

        /* time is up */
        if ((end_ts - start_ts) >= save_time) 
        {
            ak_print_normal(MODULE_ID_AI, "*** timp is up ***\n\n");
            break;
        }
    }
    ak_print_normal(MODULE_ID_AI, "*** capture finish ***\n\n");
}

/*
 * help_hint: use the -h --help option.Print option of help information
 * return: 0
 */
static int help_hint( char *pc_prog_name )
{
    int i;

    printf("%s\n" , pc_prog_name);
    for(i = 0; i < sizeof(option_long) / sizeof(struct option); i ++) 
    {
        printf("\t--%-16s -%c %s\n" , option_long[ i ].name , option_long[ i ].val , ac_option_hint[ i ]);
    }
    printf("\n\n");
    return 0;
}

char *get_option_short( struct option *p_option, int i_num_option, char *pc_option_short, 
                        int i_len_option )
{
    int i;
    int i_offset = 0;
    char c_option;

    for( i = 0 ; i < i_num_option ; i ++ ) 
    {
        c_option = p_option[ i ].val;
        switch( p_option[ i ].has_arg )
        {
        case no_argument:
            i_offset += snprintf( pc_option_short + i_offset , i_len_option - i_offset , "%c" , c_option );
            break;
        case required_argument:
            i_offset += snprintf( pc_option_short + i_offset , i_len_option - i_offset , "%c:" , c_option );
            break;
        case optional_argument:
            i_offset += snprintf( pc_option_short + i_offset , i_len_option - i_offset , "%c::" , c_option );
            break;
        }
    }
    return pc_option_short;
}

int parse_option( int argc, char **argv )
{
    int i_option;
    char ac_option_short[ LEN_OPTION_SHORT ];
    int i_array_num = sizeof( option_long ) / sizeof( struct option ) ;
    char c_flag = 1;

    if (argc < 5 && argc != 1) 
    {
        help_hint( argv[ 0 ] );
        c_flag = 0;
        goto parse_option_end;
    }

    get_option_short( option_long, i_array_num , ac_option_short , LEN_OPTION_SHORT );
    while((i_option = getopt_long(argc , argv , ac_option_short , option_long , NULL)) > 0) 
    {
        switch(i_option) 
        {
        case 'h' :                                                          //help
            help_hint( argv[ 0 ] );
            c_flag = 0;
            goto parse_option_end;
        case 't' :                                                          //second
            save_time = atoi( optarg ) * 1000 ;
            break;
        case 's' :                                                          //sample-rate
            sample_rate = atoi( optarg );
            break;
        case 'c' :                                                          //sample-rate
            channel_num = atoi( optarg );
            break;
        case 'v' :                                                          //volume
            volume = atoi( optarg );
            printf("----sample_rate- %d-----\n", sample_rate);
            break;
        case 'p' :                                                          //path
            save_path = optarg;
            break;
        }
    }
parse_option_end:
    return c_flag;
}

/**
 * Preconditions:
 * 1. T card is already mounted
 * 2. mic or linein must ready
 */
int main(int argc, char **argv)
{
    /* start the application */
    sdk_run_config config;
    config.mem_trace_flag = SDK_RUN_DEBUG;
    ak_sdk_init( &config );

    ak_print_normal(MODULE_ID_AI, "*****************************************\n");
	ak_print_normal(MODULE_ID_AI, "** ai demo version: %s **\n", ak_ai_get_version());
    ak_print_normal(MODULE_ID_AI, "*****************************************\n");
	
    if( parse_option( argc, argv ) == 0 ) 
    {
		ak_print_error_ex(MODULE_ID_AI,"parse param invalid!\n");
		return 0;
    }

    open_pcm_file(sample_rate, channel_num, save_path, &fp);

    int ret = -1;

    struct ak_audio_in_param param;
    memset(&param, 0, sizeof(struct ak_audio_in_param));
    ak_print_notice_ex(MODULE_ID_AI, "sample_rate=%d\n", sample_rate);
    param.pcm_data_attr.sample_rate = sample_rate;                // set sample rate
    param.pcm_data_attr.sample_bits = AK_AUDIO_SMPLE_BIT_16;    // sample bits only support 16 bit
    param.pcm_data_attr.channel_num = channel_num;    // channel number
    param.dev_id = 0;
     
    int ai_handle_id = -1;
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

    ai_capture_loop(ai_handle_id, save_path, save_time);

    ret = ak_ai_stop_capture(ai_handle_id);
    if (ret) 
    {
        ak_print_error(MODULE_ID_AI, "*** ak_ai_stop_capture failed. ***\n");
        ak_ai_close(ai_handle_id);
        goto exit;
    }

    ret = ak_ai_close(ai_handle_id);
    if (ret) 
    {
        ak_print_normal(MODULE_ID_AI, "*** ak_ai_close failed. ***\n");
    }

exit:
    /* close file handle */
    close_pcm_file(fp);
    ak_print_normal(MODULE_ID_AI, "******** exit ai demo ********\n");
    return ret;
}
