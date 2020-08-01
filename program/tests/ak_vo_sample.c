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

#include "ak_common_graphics.h"
#include "ak_mem.h"
#include "ak_log.h"
#include "ak_vo.h"
#include "ak_tde.h"

#define MIPI_SC_WIDTH 1280
#define MIPI_SC_HEIGHT 800

#define RGB_SC_WIDTH  1024
#define RGB_SC_HEIGHT  600

#define LEN_HINT                512
#define LEN_OPTION_SHORT        512
#define MAX_DIS_NUM             4

static char *pc_prog_name = NULL;                                                      //demo名称
static int screen_flag    = 0;                                                         //mipi屏幕
static char *data_file = "/mnt/yuv";
static char *logo_file = "/mnt/anyka.logo.577.160.rgb";
static int display_num = 4;
static AK_GP_FORMAT data_format = GP_FORMAT_YUV420P;
static AK_GP_FORMAT out_format  = GP_FORMAT_RGB888;

static char ac_option_hint[  ][ LEN_HINT ] = {                                         //操作提示数组
    "打印帮助信息" ,
    "显示屏幕类型 0 - MIPI, 1 - RGB" ,
    "yuv图像文件路径目录" ,
    "分屏数目 [1, 4]" ,
    "[NUM] 数据输入格式 0:RGB565 1:RGB888 2:BGR565 3:BGR888 4:YUV420P 5:YUV420SP 6:ARGB8888 7:RGBA8888 8:ABGR8888 9:BGRA8888 10:TILED32X4" ,
    "[NUM] 数据输出格式 0:RGB565 1:RGB888 2:BGR565 3:BGR888 ",
    "logo 文件的路径，只支持提供的anyka.logo.577.160.rgb文件显示",
    " ",
};


static struct option option_long[ ] = {
    { "help"              , no_argument       , NULL , 'h' } ,      //"打印帮助信息" ,
    { "screen-type"       , required_argument , NULL , 't' } ,      //"显示屏幕类型" ,
    { "file-dir"          , required_argument , NULL , 'f' } ,      //"文件路径" ,
    { "display-num"       , required_argument , NULL , 'n' } ,      //"分屏数目1-4" ,
    { "format-in"         , required_argument , NULL , 'i' } ,      //"[NUM] 数据输入格式 0:RGB565 1:RGB888 2:BGR565 3:BGR888 4:YUV420P 5:YUV420SP 6:ARGB8888 7:RGBA8888 8:ABGR8888 9:BGRA8888 10:TILED32X4" ,
    { "format-out"        , required_argument , NULL , 'o' } ,      //"[NUM] 数据输出格式 0:RGB565 1:RGB888 2:BGR565 3:BGR888 ,
    { "logo-file"         , required_argument , NULL , 'l' } ,      //"logo 文件的路径，只支持提供的anyka.logo.rgb文件显示"
    { 0                   , 0                 , 0    , 0   } ,
 };

/*
 * help_hint: use the -h --help option.Print option of help information
 * return: 0
 */
static void usage(const char * name)
{
    ak_print_normal(MODULE_ID_VO," %s -t [num] -f [yuv-file-dir] -n [num] -i [num] -o [num] -l [logo-dir]\n", name);
    ak_print_normal(MODULE_ID_VO,"eg: %s -t 0 -f /mnt/yuv -n 1 -i 4 -o 1 -l /mnt/anyka.logo.577.160.rgb\n", name);
    ak_print_normal(MODULE_ID_VO,"the DATA-file path should contains data-picture in the res 1920*1080 only, all the file should in the same res\n");
    ak_print_normal(MODULE_ID_VO,"the logo-dir should name anyka.logo.577.160.rgb, this file format is rgb888\n");
}

static int help_hint(void)
{
    int i;

    ak_print_normal(MODULE_ID_VO,"%s\n" , pc_prog_name);
    for(i = 0; i < sizeof(option_long) / sizeof(struct option); i ++) {
        if( option_long[ i ].val != 0 )
            ak_print_normal(MODULE_ID_VO,"\t--%-16s -%c %s\n" , option_long[ i ].name , option_long[ i ].val , ac_option_hint[ i ]);
    }

    usage(pc_prog_name);
    ak_print_normal(MODULE_ID_VO, "\n\n");
    return AK_SUCCESS;
}

/*
 * get_option_short: fill the stort option string.
 * return: option short string addr.
 */
static char *get_option_short( struct option *p_option, int i_num_option, char *pc_option_short, int i_len_option )
{
    int i;
    int i_offset = 0;
    char c_option;

    for( i = 0 ; i < i_num_option ; i ++ )
    {
        if( ( c_option = p_option[ i ].val ) == 0 )
            continue;
        
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

static int parse_option( int argc, char **argv )
{
    int i_option;

    char ac_option_short[ LEN_OPTION_SHORT ];
    int i_array_num = sizeof( option_long ) / sizeof( struct option ) ;
    char c_flag = AK_TRUE;
    pc_prog_name = argv[ 0 ];

    get_option_short( option_long, i_array_num , ac_option_short , LEN_OPTION_SHORT );
    while((i_option = getopt_long(argc , argv , ac_option_short , option_long , NULL)) > 0) {
        switch(i_option) {
            case 'h' :                                                          //help
                help_hint();
                c_flag = AK_FALSE;
                goto parse_option_end;
            case 't' :                                                          //screen type 
                screen_flag = atoi( optarg );
                break;
            case 'f' :                                                          //data file path
                data_file = optarg;
                break;
            case 'n' :                                                          //display num
                display_num = atoi( optarg );
                break;
            case 'i' :                                                          //data_file format 
                data_format = atoi(optarg);
                break;
            case 'o' :                                                          //out put format
                out_format = atoi(optarg);
                break;
            case 'l' :                                                          //logo file
                logo_file = optarg;
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

/* add the  obj to GUI layer */
static int add_logo_to_gui(void)
{
    /* get the logo file */
    FILE *fp_logo = fopen(logo_file, "rb");
    if (NULL == fp_logo)
    {
        ak_print_error_ex(MODULE_ID_VO, "logo file open failed !!! This file is %s, check it!\n", logo_file);
        return AK_FAILED;
    }
    /* get the file size */
    fseek(fp_logo, 0, SEEK_END);
    int file_size = ftell(fp_logo);
    fseek(fp_logo, 0, SEEK_SET);

    /* read data from teh file */
    void *logo_data = ak_mem_dma_alloc(MODULE_ID_VO, file_size);
    if(logo_data == NULL)
    {
        ak_print_error_ex(MODULE_ID_VO, "Can't malloc DMA memory!\n");
        return AK_FAILED;
    }
    fread(logo_data, 1, file_size, fp_logo);

    struct ak_vo_obj gui_obj;

    /* set obj src info*/	
    gui_obj.format = GP_FORMAT_RGB888;
    gui_obj.cmd = GP_OPT_BLIT;
    gui_obj.vo_layer.width = 577;
    gui_obj.vo_layer.height = 160;
    gui_obj.vo_layer.clip_pos.top = 0;
    gui_obj.vo_layer.clip_pos.left = 0;
    gui_obj.vo_layer.clip_pos.width = 577;
    gui_obj.vo_layer.clip_pos.height = 160;

    ak_mem_dma_vaddr2paddr(logo_data, &(gui_obj.vo_layer.dma_addr));

    /* set dst_layer 1 info*/
    gui_obj.dst_layer.top = 0;
    gui_obj.dst_layer.left = 0;
    gui_obj.dst_layer.width = 577;
    gui_obj.dst_layer.height = 160;

    /* display obj 1*/
    ak_vo_add_obj(&gui_obj, AK_VO_LAYER_GUI_1);

    /* free source */
    ak_mem_dma_free(logo_data);
    //posix_fadvise(fileno(fp_logo), 0, file_size, POSIX_FADV_DONTNEED);
    fclose(fp_logo);

    return AK_SUCCESS;
}

#ifdef AK_RTOS
static int ak_vo_sample(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    /* start the application */
    sdk_run_config config;
    config.mem_trace_flag = SDK_RUN_DEBUG;
    ak_sdk_init( &config );

    //print the version
    ak_print_normal(MODULE_ID_VI, "*****************************************\n");
    ak_print_normal(MODULE_ID_VI, "** vo version: %s **\n", ak_vo_get_version());
    ak_print_normal(MODULE_ID_VI, "*****************************************\n");

    /* parse the option */
    if( parse_option( argc, argv ) == AK_FALSE )            //解释和配置选项
    {
        return AK_FAILED;                                           //打印帮助后退出
    }

    if (display_num < 0 || display_num > MAX_DIS_NUM)
    {
        ak_print_error_ex(MODULE_ID_VO, "display num is only support 0 to %d\n", MAX_DIS_NUM);
        return AK_FAILED;
    }

     /* store the file path */
    char file_path[64]="";
    int ret = 0;
    /* get the res and rotation */
    int dst_width = 0;
    int dst_height = 0;
    int rotate = 0;

    /* get the screen res */
    if (screen_flag == 1)
    {
        /* 1 means for RGB */
        dst_width = RGB_SC_WIDTH;
        dst_height = RGB_SC_HEIGHT;
        rotate    = AK_GP_ROTATE_NONE;
    }
    else if (screen_flag == 0)
    {
        /* 0 means for MIPI */
        dst_width = MIPI_SC_WIDTH;
        dst_height = MIPI_SC_HEIGHT;
        rotate = AK_GP_ROTATE_90;
    }
    else
    {
        ak_print_error_ex(MODULE_ID_VO, "screen type is invalid\n");
        return AK_FAILED;
    }

    /* fill the struct to open the vo */
    struct ak_vo_param	param;

    /* fill the struct ready to open */
    param.width = dst_width;//1280;
    param.height = dst_height;// 800;
    param.format = out_format;  //format to output
    param.rotate = rotate;    //rotate value

    /* open vo */
    ret = ak_vo_open(&param, DEV_NUM);      //open vo

    if(ret != 0)
    {
        /* open failed return -1 */
        ak_print_error_ex(MODULE_ID_VO, "ak_vo_open failed![%d]\n",ret);
        return AK_FAILED;	
    }

    /* create the video layer */
    struct ak_vo_layer_in video_layer;
    video_layer.create_layer.height = dst_height;   //800; //get the res
    video_layer.create_layer.width  = dst_width;    //1280; //get the res
    /* just from (0,0) */
    video_layer.create_layer.left  = 0;         //layer left pos
    video_layer.create_layer.top   = 0;         // layer top pos
    video_layer.layer_opt          = 0;
    video_layer.format             = out_format;

    ret = ak_vo_create_video_layer(&video_layer, AK_VO_LAYER_VIDEO_1);
    if(ret != 0)
    {
        /* if failed, close the vo */
        ak_print_error_ex(MODULE_ID_VO, "ak_vo_create_video_layer failed![%d]\n",ret);
        ak_vo_close(DEV_NUM);       // if failed, close the vo
        return AK_FAILED;	
    }

    /* create the gui layer */
    struct ak_vo_layer_in gui_layer;
    /* only support the given log file */
    gui_layer.create_layer.height = 160;    /* logo res */
    gui_layer.create_layer.width  = 577;    /* logo res */
    gui_layer.create_layer.left  = 0;       /* logo pos */
    gui_layer.create_layer.top   = 0;       /* logo pos */
    gui_layer.format             = out_format;  /* gui layer format */

    gui_layer.layer_opt          = GP_OPT_COLORKEY; /* support the colorkey opt */

    gui_layer.colorkey.coloract = COLOR_DELETE;       /* delete the color */
    gui_layer.colorkey.color_min = 0xFFFF00;        /* min value */
    gui_layer.colorkey.color_max = 0xFFFFFF;        /* max value */

    struct ak_vo_layer_out gui_info;                /* output the gui layer info */
    ret = ak_vo_create_gui_layer(&gui_layer, AK_VO_LAYER_GUI_1, &gui_info);
    if(ret != 0)
    {
        ak_print_error_ex(MODULE_ID_VO, "ak_vo_create_gui_layer failed![%d]\n",ret);
        ak_vo_destroy_layer(AK_VO_LAYER_VIDEO_1);
        ak_vo_close(DEV_NUM);
        return AK_FAILED;	
    }

    /* use the double buff mode */
    ak_vo_set_fbuffer_mode(AK_VO_BUFF_DOUBLE);

    /* support the yuv 420p */
	int len = (1920*1080*3)/2;

    /* get the size of the pixel */
    switch (data_format)
    {
    /* 565 */
    case GP_FORMAT_RGB565:
    case GP_FORMAT_BGR565:
        len = 1920*1080*2;
        break;
    /* 888 */
    case GP_FORMAT_BGR888:
    case GP_FORMAT_RGB888:
        len = 1920*1080*3;
        break;
    /* 8888 */
    case GP_FORMAT_ABGR8888:
    case GP_FORMAT_ARGB8888:
    case GP_FORMAT_RGBA8888:
    case GP_FORMAT_BGRA8888:
        len = 1920*1080*4;
        break;
    /* YUV */
    case GP_FORMAT_YUV420P:
    case GP_FORMAT_YUV420SP:
        break;
    default:
        ak_print_error_ex(MODULE_ID_VO, "here not support the tileyuv data\n");
        
    }

    void *data = ak_mem_dma_alloc(MODULE_ID_VO, len);
    if(data == NULL)
    {
        ak_print_error_ex(MODULE_ID_VO, "Can't malloc DMA memory!\n");
        ret = AK_FAILED;
        goto err;
    }

    /* 1-4 partion screen to show the picture */
    int dou = 1;
    if (display_num == 1)
        dou = 1;
    else
        dou = 2;

    /* add logo to gui layer */
    ret = add_logo_to_gui();
    if(ret != 0)
    {
        ak_print_error_ex(MODULE_ID_VO, "add_logo_to_gui failed![%d]\n",ret);
        goto err;	
    }

    /* scan the all file on the dir */
    DIR *dir = NULL;
    struct dirent *entry = NULL;

    dir = opendir(data_file);//
    if (dir == NULL)
    {
        ak_print_error_ex(MODULE_ID_VO, "can't open file dir[%s]\n", data_file);
        ret = AK_FAILED;
        goto err;      
    }

    /* each file should put on the screen */
    while ((entry = readdir(dir)))
    {
        if (entry->d_type == 8)
        {
            memset(file_path, 0, sizeof(file_path));
            sprintf(file_path, "%s/%s",  data_file, entry->d_name);  

            FILE *fp = fopen(file_path, "rb");
            if(fp!= NULL)
            {
                int ret = 0;
                memset(data, 0, len);
                ret = fread(data, 1, len, fp);      // read the file
                ak_print_normal(MODULE_ID_VO, "read [%d] byte to dma buffer\n",ret);

                /* obj add */
                struct ak_vo_obj obj;

                /* set obj src info*/	
                obj.format = data_format;
                obj.cmd = GP_OPT_SCALE;
                obj.vo_layer.width = 1920;
                obj.vo_layer.height = 1080;
                obj.vo_layer.clip_pos.top = 0;
                obj.vo_layer.clip_pos.left = 0;
                obj.vo_layer.clip_pos.width = 1920;
                obj.vo_layer.clip_pos.height = 1080;

                ak_mem_dma_vaddr2paddr(data, &(obj.vo_layer.dma_addr));

                /* show as the screen partion set */
                int counter = display_num;
                if (display_num == 1)
                {
                    /* set dst_layer 1 info*/
                    obj.dst_layer.top = 0;
                    obj.dst_layer.left = 0;
                    obj.dst_layer.width = dst_width;
                    obj.dst_layer.height = dst_height;
                    /* display obj 1*/
                    ak_vo_add_obj(&obj, AK_VO_LAYER_VIDEO_1);
                }
                else
                {
                    /* more than one partion */
                    if (counter)
                    {
                        /* set dst_layer 1 info*/
                        obj.dst_layer.top = 0;
                        obj.dst_layer.left = 0;
                        obj.dst_layer.width = dst_width/dou;    
                        obj.dst_layer.height = dst_height/dou;
                        /* display obj 1*/
                        ak_vo_add_obj(&obj, AK_VO_LAYER_VIDEO_1);
                        counter--;
                    }

                    if (counter != 0)
                    {
                        /* set dst_layer 2 info*/
                        obj.dst_layer.top = 0;
                        obj.dst_layer.left = dst_width/dou;
                        obj.dst_layer.width = dst_width/dou;
                        obj.dst_layer.height = dst_height/dou;//400;
                        /* display obj 1*/
                        ak_vo_add_obj(&obj, AK_VO_LAYER_VIDEO_1);
                        counter--;
                    }

                    if (counter)
                    {
                        /* set dst_layer 3 info*/
                        obj.dst_layer.top = dst_height/dou;//400;
                        obj.dst_layer.left = 0;
                        obj.dst_layer.width = dst_width/dou;
                        obj.dst_layer.height = dst_height/dou;//400;
                        /* display obj 1*/
                        ak_vo_add_obj(&obj, AK_VO_LAYER_VIDEO_1);
                        counter--;
                    }

                    if (counter)
                    {
                        /* set dst_layer 4 info*/
                        obj.dst_layer.top = dst_height/dou;//400;
                        obj.dst_layer.left = dst_width/dou;
                        obj.dst_layer.width = dst_width/dou;
                        obj.dst_layer.height = dst_height/dou;//400;
                        /* display obj 1*/
                        ak_vo_add_obj(&obj, AK_VO_LAYER_VIDEO_1);
                        counter--;
                    }

                }

                /* flush to screen */
                int cmd = AK_VO_REFRESH_VIDEO_GROUP & 0x100;
                cmd |= AK_VO_REFRESH_GUI_GROUP & 0x10000;
                ak_vo_refresh_screen(cmd);

                /* finish one file show */
                //posix_fadvise(fileno(fp), 0, len, POSIX_FADV_DONTNEED);
                fclose(fp);
            }
            else
            {
                ak_print_error_ex(MODULE_ID_VO, "open file [%s] filed!\n", file_path);	
            }
        }
        ret = AK_SUCCESS;
    }

err:
    /* if err or finish, release the src */
    if(data)
        ak_mem_dma_free(data);

    /* destroy the layer, release the src */
    ak_vo_destroy_layer(AK_VO_LAYER_VIDEO_1);
    ak_vo_destroy_layer(AK_VO_LAYER_GUI_1);

    /* close the vo */
    ak_vo_close(DEV_NUM);

    return ret;
}

#ifdef AK_RTOS
MSH_CMD_EXPORT(ak_vo_sample, vo sample)
#endif

