#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include "ak_common.h"
#include "ak_log.h"
#include "ak_common_graphics.h"
#include "ak_vdec.h"
#include "ak_vo.h"
#include "ak_thread.h"
#include "ak_mem.h"

#ifdef AK_RTOS
#include "rtthread.h"
#define THREAD_PRIO 90
#else
#define THREAD_PRIO -1
#endif

#define RECORD_READ_LEN      1024*100 /* read video file buffer length */
#define DE_VIDEO_SIZE_MAX    5
/* this is length for parse */
#define LEN_HINT                512
#define LEN_OPTION_SHORT        512

/* mipi screen res */
#define MIPI_SC_WIDTH 1280
#define MIPI_SC_HEIGHT 800

/* RGB screen res */
#define RGB_SC_WIDTH  1024
#define RGB_SC_HEIGHT  600

#define MAX_DE_NUM   4

/* resolation define */
struct resolution_t {
	unsigned int width;
	unsigned int height;
	unsigned char str[20];
};

static char *pc_prog_name = NULL;                      //demo名称
static char *file  = NULL;                              //file name
static char *type         = NULL;                      //get the type input
static int screen         = 0;                         //mipi屏幕
static int refresh_flag   = 0;                         //flag to refresh
static int res            = 3;
static int refresh_record_flag = 0;                    //flag to refresh
static int decoder_num = 1;
static int handle_id[MAX_DE_NUM] = { -1, -1, -1, -1};  //vdec handle id
ak_mutex_t refresh_flag_lock;
struct ak_layer_pos  obj_pos[MAX_DE_NUM];              //store the pos

/* decoder resolution */
static struct resolution_t resolutions[DE_VIDEO_SIZE_MAX] = {
    {640,   360,   "DE_VIDEO_SIZE_360P"},
    {640,   480,   "DE_VIDEO_SIZE_VGA"},
    {1280,  720,   "DE_VIDEO_SIZE_720P"},
    {1920,	1080,  "DE_VIDEO_SIZE_1080P"},
    {2560,  1920,  "DE_VIDEO_SIZE_1920P"}
};

/* this is the message to print */
char ac_option_hint[  ][ LEN_HINT ] = {                                         //操作提示数组
    "打印帮助信息" ,
    "屏幕类型 0 - mipi, 1 - rgb",
    "解码的数�?：[1-4]",
    "解码视频文件路径或JPEG文件目录路径" ,
    "解码数据格式 val：h264 h265 jpeg",
    "码流分辨率，max�?560*1920",
    "",
};

/* opt for print the message */
struct option option_long[ ] = {
    { "help"              , no_argument       , NULL , 'h' } ,      //"打印帮助信息" ,
    { "screen"            , required_argument , NULL , 't' } ,      //"屏幕类型" ,
    { "decode-num"        , required_argument , NULL , 'n' } ,      //"解码数量" ,
    { "file-dir"          , required_argument , NULL , 'f' } ,      //"文件路径" ,
    { "format-in"         , required_argument , NULL , 'i' } ,      //"数据格式" ,
    { "resolution"        , required_argument , NULL , 'r' } ,      //"分辨�? ,
    { 0                   , 0                 , 0    , 0   } ,
};

/*
* get_option_short: fill the stort option string.
* return: option short string addr.
*/
char *get_option_short( struct option *p_option, int i_num_option, char *pc_option_short, int i_len_option )
{
    int i;
    int i_offset = 0;
    char c_option;

    /* get the option */
    for( i = 0 ; i < i_num_option ; i ++ )
    {
        if( ( c_option = p_option[ i ].val ) == 0 ) 
        {
            continue;
        }

        switch( p_option[ i ].has_arg )
        {
        case no_argument:
            /* if no argument, set the offset for default */
            i_offset += snprintf( pc_option_short + i_offset , i_len_option - i_offset , "%c" , c_option );
            break;
        case required_argument:
            /* required argument offset calculate */
            i_offset += snprintf( pc_option_short + i_offset , i_len_option - i_offset , "%c:" , c_option );
            break;
        case optional_argument:
            /* calculate the option offset */
            i_offset += snprintf( pc_option_short + i_offset , i_len_option - i_offset , "%c::" , c_option );
            break;
        }
    }
    return pc_option_short;
}

void usage(const char * name)
{
    /* a example for this sample in usage */
    ak_print_normal(MODULE_ID_VDEC, "usage: %s -t 0 or 1 -n 1-4 -f [FILE_path]  -i [type] -r res\n", name);
    ak_print_normal(MODULE_ID_VDEC, "eg.: %s -t 0 -n 1 -f /mnt/test.str -i h264 -r 1920*1080\n", name);
    ak_print_normal(MODULE_ID_VDEC, "screen -t:     0 or 1\n");
    ak_print_normal(MODULE_ID_VDEC, "decode-num -n:     1-4\n");
    ak_print_normal(MODULE_ID_VDEC, "format-in -i:          h264,  h265,  or  jpeg\n");
    ak_print_normal(MODULE_ID_VDEC, "resolution -r:  value 0 ~ 4\n");
    ak_print_normal(MODULE_ID_VDEC, "                      0 - 640*360\n");
    ak_print_normal(MODULE_ID_VDEC, "                      1 - 640*480\n");
    ak_print_normal(MODULE_ID_VDEC, "                      2 - 1280*720\n");
    ak_print_normal(MODULE_ID_VDEC, "                      3 - 1920*1080\n");
    ak_print_normal(MODULE_ID_VDEC, "                      4 - 2560*1920\n");
    ak_print_normal(MODULE_ID_VDEC, "NOTE: jpeg file_path should be a directory\n"
            "\teg: if you have several jpeg pictures in /mnt/picture,you should set the FILE_path value as /mnt/picture\n");
}

/* if opt is not supported, print the help message */
static int help_hint(void)
{
    int i;
 
    ak_print_normal(MODULE_ID_VDEC, "%s\n" , pc_prog_name);
    /* parse the all supported option */
    for(i = 0; i < sizeof(option_long) / sizeof(struct option); i ++)
    {
        if( option_long[ i ].val != 0 ) 
            ak_print_normal(MODULE_ID_VDEC, "\t--%-16s -%c %s\n" ,
                                                option_long[ i ].name , option_long[ i ].val , ac_option_hint[ i ]);
    }
    usage(pc_prog_name);
    ak_print_normal(MODULE_ID_VDEC, "\n\n");
    return 0;
}

/* parse the option from cmd line */
int parse_option( int argc, char **argv )
{
    int i_option;
 
    char ac_option_short[ LEN_OPTION_SHORT ];
    int i_array_num = sizeof( option_long ) / sizeof( struct option ) ; /* get the option num*/
    char c_flag = AK_TRUE;
    pc_prog_name = argv[ 0 ];   /* get the option */

    get_option_short( option_long, i_array_num , ac_option_short , LEN_OPTION_SHORT );  /* parse the cmd line input */
    while((i_option = getopt_long(argc , argv , ac_option_short , option_long , NULL)) > 0)
    {
        switch(i_option)
        {
        case 'h' :                                                          //help
            help_hint();
            c_flag = AK_FALSE;
            goto parse_option_end;
        case 't' :                                                          //screen format 
            screen = atoi( optarg );
            break;
        case 'f' :                                                          //data file path
            file = optarg;
            break;
        case 'i' :                                                          //data_file format 
            type = optarg;
            break;
        case 'n' :                                                          //decode num 
            decoder_num = atoi( optarg );
            break;
        case 'r' :                                                          //decode num 
            res = atoi( optarg );
            break;
        default :
            help_hint();
            c_flag = AK_FALSE;
            goto parse_option_end;
        }
    }

parse_option_end:
    return c_flag;
}

/**
 * decode_stream - send the stream data to decode
 * @adec_handle[IN]: adec handle
 * @data[IN]: audio file data
 * @read_len[IN]: audio file data length
 * notes:
 */
static void decode_stream(int handle_id, unsigned char *data, int read_len)
{
    int send_len = 0;
    int dec_len = 0;
    int ret = 0;

    /* cycle to send the data to decode */
    while (read_len > 0)
    {
        /* send stream to decode */
        ret = ak_vdec_send_stream(handle_id, &data[send_len], read_len, 1, &dec_len);
        if (ret !=  0)
        {
            ak_print_error_ex(MODULE_ID_VDEC, "write video data failed!\n");
            break;
        }

        /* record the length to send to decoder */
        read_len -= dec_len;
        send_len += dec_len;
    }
}

/* show the stream to screen */
int demo_play_func(struct ak_vdec_frame *frame)
{
    /* set the pos in the screen */
    int pos_top = obj_pos[frame->id].top;
    int pos_left = obj_pos[frame->id].left;
    int width = obj_pos[frame->id].width;
    int height = obj_pos[frame->id].height;

    /* obj to add to layer */
    struct ak_vo_obj    obj;
    memset(&obj, 0x00, sizeof(struct ak_vo_obj));
    if (frame->data_type == AK_TILE_YUV)
    {
        /* if the data type is tileyuv */
        obj.format = GP_FORMAT_TILED32X4;
        obj.vo_tileyuv.data = frame->tileyuv_data;
    }
    else
    {
        /* if the data type is yuv420sp */
        obj.format = GP_FORMAT_YUV420SP;
        obj.cmd = GP_OPT_SCALE;         /* scale to screen */
        obj.vo_layer.width = frame->yuv_data.pitch_width;   /* the real width */
        obj.vo_layer.height = frame->yuv_data.pitch_height; /* the real height */

        /* pos and range from the src */
        obj.vo_layer.clip_pos.top = 0;
        obj.vo_layer.clip_pos.left = 0;
        obj.vo_layer.clip_pos.width = frame->width;
        obj.vo_layer.clip_pos.height = frame->height;
        ak_mem_dma_vaddr2paddr(frame->yuv_data.data, &(obj.vo_layer.dma_addr));
    }

    /* pos and range for the dst layer to contain the src */
    obj.dst_layer.top = pos_top;
    obj.dst_layer.left = pos_left;
    obj.dst_layer.width = width;
    obj.dst_layer.height = height;

    /* add this picture to layer */
    ak_vo_add_obj(&obj, AK_VO_LAYER_VIDEO_1);

    ak_thread_mutex_lock(&refresh_flag_lock);
    /* set the refresh flag */
    refresh_flag |= (0x01 << frame->id);

    /* get the value */
    if(refresh_flag == refresh_record_flag)
    {
        refresh_flag = 0;
        /* video_1 is created,only flush the layer */
        int cmd = AK_VO_REFRESH_VIDEO_GROUP & 0x100;
        ak_vo_refresh_screen(cmd);
    }
    ak_thread_mutex_unlock(&refresh_flag_lock);

    /* wait for all decoder ready */
    while ( (refresh_flag != refresh_record_flag) && (refresh_flag & (0x01 << frame->id)) )
    {
        ak_sleep_ms(5);
    }

    return 0;
}

void *vdec_send_decode_th(void *arg)
{
    ak_thread_set_name("vdec_demo_test");

    int *handle_id = (int *)arg;
    char *file_name = file;

    /* open video file */
    FILE *fp = fopen(file_name, "r");
    if(NULL == fp) {
        ak_print_error_ex(MODULE_ID_VDEC, "open %s failed\n", file_name);
        return NULL;
    }
    ak_print_normal(MODULE_ID_VDEC, "open record file: %s OK\n", file_name);

    /* read video file stream and send to decode, */
    int read_len = 0;
    int total_len = 0;
    unsigned char *data = (unsigned char *)ak_mem_alloc(MODULE_ID_VDEC, RECORD_READ_LEN);

    /* set file from the beginning */
    fseek(fp, 0, SEEK_SET);

    /* loop for sending data to decode */
    do
    {
        /* read the record file stream */
        memset(data, 0x00, RECORD_READ_LEN);
        read_len = fread(data, sizeof(char), RECORD_READ_LEN, fp);

        /* get the data and send to decoder */
        if(read_len > 0)
        {
            total_len += read_len;
            /* play loop */
            decode_stream(*handle_id, data, read_len);
            ak_sleep_ms(10);
        }
        else if(0 == read_len)
        {
            /* read to the end of file */
            ak_print_normal_ex(MODULE_ID_VDEC, "\n\tread to the end of file\n");
            break;
        }
        else 
        {
            /* if the data is wrong */
            ak_print_error_ex(MODULE_ID_VDEC, "\nread file error\n");
            break;
        }
    }while(1);

    /* if finish, notice the decoder for data sending finish */
    ak_vdec_end_stream(*handle_id);
    if (data != NULL)
        ak_mem_free(data);
#ifndef AK_RTOS
    posix_fadvise(fileno(fp), 0, total_len, POSIX_FADV_DONTNEED);
#endif
    fclose(fp);

    ak_print_normal_ex(MODULE_ID_VDEC, "send stream th exit\n");
    return NULL;
}

void *vdec_solo_frame(void *arg)
{
    char file_name[128];
    int ret = -1;
    int *handle_id = (int *)arg;

    /* read video file stream and send to decode, */
    int read_len = 0;
    int total_len = 0;
    unsigned char *data = NULL;
    char *dir_name = file;

    /* a dir opt */
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    /* this dir only contain the jpeg file */
    dir = opendir(dir_name);//
    if (dir == NULL)
    {
        ak_print_error_ex(MODULE_ID_VDEC, "can't open file dir[%s]\n", dir_name);
        goto jpeg_exit;
    }

    /* a loop for read all the jpeg file */
    while ((entry = readdir(dir)))
    {
        /* if get the file */
        if (entry->d_type == DT_REG)
        {
            /* open video file */
            sprintf(file_name, "%s/%s",  dir_name, entry->d_name);
            FILE *fp = fopen(file_name, "r");
            if(NULL == fp)
            {
                ak_print_error_ex(MODULE_ID_VDEC, "open %s failed\n", file_name);
                continue;      
            }
            ak_print_normal(MODULE_ID_VDEC, "open record file: %s OK\n", file_name);

            /* get the file size */
            fseek(fp, 0, SEEK_END);
            int file_size = ftell(fp);

            data = (unsigned char *)ak_mem_alloc(MODULE_ID_VDEC, file_size);

            /* set file from the beginning */
            fseek(fp, 0, SEEK_SET);

            /* read the record file stream */
            memset(data, 0x00, sizeof(data));
            read_len = fread(data, sizeof(char), file_size, fp);

            /* send the data to decode */
            ak_print_normal(MODULE_ID_VDEC, "size is [%d] \n", read_len);
            if(read_len > 0)
            {
                total_len += read_len;
                /* play loop */
                decode_stream(*handle_id, data, read_len);
                ak_sleep_ms(10);
            }
            else if(0 == read_len)
            {
                /* read to the end of file */
                ak_print_normal_ex(MODULE_ID_VDEC, "\n\tread to the end of file\n");
                goto file_failed;
            }
            else 
            {
                /* error is not support */
                ak_print_error_ex(MODULE_ID_VDEC, "read file error\n");
                goto file_failed;
            }

            /* inform that sending data is over */
            ak_vdec_end_stream(*handle_id);

            struct ak_vdec_frame frame;
            /* need to get the frame for a while */
            while (1)
            {
                /* get the frame */
                ret = ak_vdec_get_frame(*handle_id, &frame);
                if(ret == 0)
                {
                    /* set frame status */
                    demo_play_func(&frame);

                    /* relase the frame and push back to decoder */
                    ak_vdec_release_frame(*handle_id, &frame);
                    break;
                }
                else
                {
                    ak_print_normal_ex(MODULE_ID_VDEC, "id [%d] get frame failed! waiting 5 ms!\n", *handle_id);
                    ak_sleep_ms(5);
                }
            }

file_failed:
#ifndef AK_RTOS
            posix_fadvise(fileno(fp), 0, file_size, POSIX_FADV_DONTNEED);
#endif
            fclose(fp);
            if (NULL != data)
                ak_mem_free(data);
        }
    }

    /* finished */
    closedir(dir);

jpeg_exit:
    ak_print_normal_ex(MODULE_ID_VDEC, "JPEG_sending is done, decoder [%d] send jpeg th exit\n", *handle_id);
    decoder_num--;
    ak_thread_mutex_lock(&refresh_flag_lock);
    //clear_bit
    refresh_record_flag &= ~(1 << (*handle_id));
    refresh_flag &= ~(1 << (*handle_id));
    ak_thread_mutex_unlock(&refresh_flag_lock);
    return NULL;
}

void *vdec_get_frame_th(void *arg)
{
    ak_thread_set_name("vdec_get_frame");
    int *id =(int *)arg;
    int ret = -1;
    int status = 0;

    /* a loop for getting the frame and display */
    do{
        /* get frame */
        struct ak_vdec_frame frame;
        ret = ak_vdec_get_frame(*id, &frame);

        if(ret == 0)
        {
            /* invoke the callback function to process the frame*/
            demo_play_func(&frame);

            /* relase the frame and push back to decoder */
            ak_vdec_release_frame(*id, &frame);
        }
        else
        {
            /* get frame failed , sleep 10ms before next cycle*/
            ak_print_normal_ex(MODULE_ID_VDEC, "id [%d] get frame failed! waiting for 10 ms\n", *id);
            ak_sleep_ms(10);
        }

        /* check the status finished */
        ak_vdec_get_decode_finish(*id, &status);
        /* true means finished */
        if (status)
        {
            decoder_num--;  //that means current decoder is finished
            ak_thread_mutex_lock(&refresh_flag_lock);
            //clear_bit
            refresh_record_flag &= ~(1 << *id);
            refresh_flag &= ~(1 << *id);
            ak_thread_mutex_unlock(&refresh_flag_lock);
            ak_print_normal(MODULE_ID_VDEC, "id is [%d] status [%d]\n", *id, status);
            return NULL;
        }
    }while(1);
}

/* this func is used to set the obj pos in layer */
int set_obj_pos(int decoder, int max_width, int max_height)
{
    switch (decoder)
    {
    case 1:
        obj_pos[0].top = 0;
        obj_pos[0].left = 0;
        obj_pos[0].width = max_width;
        obj_pos[0].height = max_height;
        break;
    case 2:
    case 3:
    case 4:
        obj_pos[0].top = 0;
        obj_pos[0].left = 0;
        obj_pos[0].width = max_width/2;
        obj_pos[0].height = max_height/2;

        obj_pos[1].top = 0;
        obj_pos[1].left = max_width/2;
        obj_pos[1].width = max_width/2;
        obj_pos[1].height = max_height/2;

        obj_pos[2].top = max_height/2;
        obj_pos[2].left = 0;
        obj_pos[2].width = max_width/2;
        obj_pos[2].height = max_height/2;

        obj_pos[3].top = max_height/2;
        obj_pos[3].left = max_width/2;
        obj_pos[3].width = max_width/2;
        obj_pos[3].height = max_height/2;
        break;
    default :
        return AK_FAILED;
    }

    return AK_SUCCESS;
}


/**
 * note: read the appointed video file, decode, and then output to vo  to playing
 */
#ifdef AK_RTOS
int vdec_sample_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    /* init sdk running */
    sdk_run_config config;
    config.mem_trace_flag = SDK_RUN_DEBUG;
    ak_sdk_init( &config );

    ak_print_normal(MODULE_ID_VI, "*****************************************\n");
    ak_print_normal(MODULE_ID_VI, "** vdec version: %s **\n", ak_vdec_get_version());
    ak_print_normal(MODULE_ID_VI, "*****************************************\n");

    /* start to parse the opt */
    if( parse_option( argc, argv ) == AK_FALSE )
    {                                                                           //解释和配置选项
        return AK_FAILED;
    }

    /* SUPPORT the 4 decoder open */
    if (decoder_num > MAX_DE_NUM || decoder_num < 1)
    {
        ak_print_error_ex(MODULE_ID_VDEC, "Only support the max [%d] decoder\n", MAX_DE_NUM);
        return AK_FAILED;
    }

    /* SUPPORT the 1 decoder open 2560*1920 */
    if (decoder_num > 1 && (res == DE_VIDEO_SIZE_MAX-1))
    {
        ak_print_error_ex(MODULE_ID_VDEC, "Only support 1 decoder for 2560*1920\n");
        return AK_FAILED;
    }

    /* screen res */
    int dst_width = 0;
    int dst_height = 0;
    int rotate = 0;
    int ret = 0;
    if (screen == 1)
    {
        /* if use the RGB screen */
        dst_width = RGB_SC_WIDTH;
        dst_height = RGB_SC_HEIGHT;
        rotate    = AK_GP_ROTATE_NONE;
    }
    else if (screen == 0)
    {
        /* if use the MIPI screen */
        dst_width = MIPI_SC_WIDTH;
        dst_height = MIPI_SC_HEIGHT;
        rotate = AK_GP_ROTATE_90;
    }
    else
    {
        ak_print_error_ex(MODULE_ID_VDEC, "check the screen type you input!!!\n");
        return AK_FAILED;
    }

    /* open vo device*/
    struct ak_vo_param vo_param = {0};
    vo_param.width = dst_width;
    vo_param.height = dst_height;
    vo_param.format = GP_FORMAT_RGB888;     //set to RGB888 output
    vo_param.rotate = rotate;

    /* start to open the vo */
    ret = ak_vo_open(&vo_param, DEV_NUM);
    if(ret != 0)
    {
        ak_print_error_ex(MODULE_ID_VDEC, "ak_vo_open failed![%d]\n", ret);
        return ret;
    }

    /* create the video layer */
    struct ak_vo_layer_in video_layer;
    video_layer.create_layer.height = dst_height;   //layer size
    video_layer.create_layer.width  = dst_width;    //layer size
    video_layer.create_layer.left  = 0;             //layer pos 
    video_layer.create_layer.top   = 0;             //layer pos
    video_layer.layer_opt          = 0;             //opt
    ret = ak_vo_create_video_layer(&video_layer, AK_VO_LAYER_VIDEO_1);
    if(ret != 0)
    {
        ak_print_error_ex(MODULE_ID_VDEC, "ak_vo_create_video_layer failed![%d]\n", ret);
        ak_vo_close(DEV_NUM);
        return ret;
    }

    /* set the obj pos in layer */
    if (set_obj_pos(decoder_num, dst_width, dst_height))
        goto err;

    ak_thread_mutex_init(&refresh_flag_lock, NULL);

    int intype = 0;
    int outtype = 0;
    int mode = 0;
    int i = 0;
    int ready_open = decoder_num;

    while (i < ready_open)
    {
        /* get the file */
        if (file == NULL)
        {
            ak_print_error_ex(MODULE_ID_VDEC, "please input the file\n");

            /* print the hint and notice */
            help_hint();
            usage(argv[0]);
            goto err;
        }
        else
            ak_print_normal_ex(MODULE_ID_VDEC, "file %s\n", file);

        /* get the type for output */
        if (type == NULL)
        {
            ak_print_error_ex(MODULE_ID_VDEC, "please input the type\n");
            /* print the hint and notice */
            help_hint();
            usage(argv[0]);
            goto err;
        }

        /* get the type for input */
        if(!strcmp(type,"h264"))
        {
            /* h264 */
            ak_print_normal(MODULE_ID_VDEC, "h264 success\n");
            intype = AK_CODEC_H264;
        }
        else if(!strcmp(type,"h265"))
        {   
            /* h265 */
            ak_print_normal(MODULE_ID_VDEC, "h265 success\n");
            intype = AK_CODEC_H265;
        }
        else if(!strcmp(type,"jpeg"))
        {
            /* jpeg */
            intype = AK_CODEC_MJPEG;
            outtype = AK_YUV420SP;
            mode = 1;
        }
        else
            ak_print_normal_ex(MODULE_ID_VDEC, "unsupport video decode input type [%s] \n", type);

        /* output vdec data set */
        if(intype != AK_CODEC_MJPEG)
            outtype = AK_TILE_YUV;

        /* open vdec */
        struct ak_vdec_param param = {0};
        param.vdec_type = intype;
        param.sc_height = resolutions[res].height;         //vdec height res set
        param.sc_width = resolutions[res].width;          //vdec width res set
        param.output_type = outtype;

        /* open the vdec */
        ret = ak_vdec_open(&param, &handle_id[i]);
        if(ret != 0)
        {
            ak_print_error_ex(MODULE_ID_VDEC, "ak_vdec_open failed!\n");

            /* destroy the layer */
            goto err;
        }
        
        ak_thread_mutex_lock(&refresh_flag_lock);
        /* refresh flag to record */
        refresh_record_flag |= (1 << handle_id[i]);
        ak_thread_mutex_unlock(&refresh_flag_lock);

        /* mode == 1 means the solo frame decode */
        if (mode)
        {   
            /* if decode the jpeg data */
            ak_pthread_t jpeg_stream_th;
            ak_thread_create(&jpeg_stream_th, vdec_solo_frame, &handle_id[i], ANYKA_THREAD_MIN_STACK_SIZE, THREAD_PRIO);
        }
        else
        {
            /* h264 or h265 decode, there is two thread */
            /* get frame thread */
            ak_pthread_t tid_stream_th;
            ak_thread_create(&tid_stream_th, vdec_get_frame_th, &handle_id[i], ANYKA_THREAD_MIN_STACK_SIZE, THREAD_PRIO);

            /* send stream thread */
            ak_pthread_t pid;
            ak_thread_create(&pid, vdec_send_decode_th, &handle_id[i], ANYKA_THREAD_MIN_STACK_SIZE, THREAD_PRIO);	
        }
        i++;
    }

    while(decoder_num)
    {
        ak_sleep_ms(500);
    }

err:
    /* close the vdec and vo, release the src */
    i = 0;
    while (i < MAX_DE_NUM && handle_id[i] != -1)
    {
        ak_vdec_close(handle_id[i]);
        i++;
    }

    /* destroy the layer and close vo */
    ak_vo_destroy_layer(AK_VO_LAYER_VIDEO_1);
    ak_vo_close(DEV_NUM);
    ak_thread_mutex_destroy(&refresh_flag_lock);

    return ret;
}

#ifdef AK_RTOS
MSH_CMD_EXPORT(vdec_sample_main, Video Decode Sample Program);
#endif

/* end of file */

