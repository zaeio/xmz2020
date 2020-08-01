#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "ak_ao.h"
#include "ak_common.h"
#include "ak_log.h"
#include "ak_mem.h"

#define PCM_READ_LEN             4096
#define AO_DAC_MAX_VOLUME        12        /* max volume according to ao */
#define TEST_AO_VOLUME            0        /* test audio volume */
#define LEN_HINT                512
#define LEN_OPTION_SHORT        512

int sample_rate = 8000;
int volume = 3;
char *pcm_file= "/mnt/ak_ao_test.pcm";
int channel_num = AUDIO_CHANNEL_MONO;

/*     
    ***********************************
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
    "[NUM]  采样率[ 8000 12000 11025 16000 22050 24000 32000 44100 48000]" ,
    "[NUM]  通道数[1 2]" ,
    "[NUM]  音量[-infinity~+infinity]" ,
    "[FILE] 播放的文件名称" ,
};

struct option option_long[ ] = {
    { "help"        , no_argument       , NULL , 'h' } , //"       打印帮助信息" ,
    { "sample-rate" , required_argument , NULL , 's' } , //"[NUM]  采样率[8000 16000 32000 44100 48000]" ,
    { "channel-number" , required_argument , NULL , 'c' } , //"[NUM]  通道数[1 2]" ,
    { "volume"      , required_argument , NULL , 'v' } , //"[NUM]  音量[0-12]" ,
    { "pcm-file"    , required_argument , NULL , 'p' } , //"[FILE] 播放的文件名称" ,
    {0, 0, 0, 0}
};

static FILE* open_pcm_file(const char *pcm_file)
{
    FILE *fp = fopen(pcm_file, "r");
    if(NULL == fp) {
        printf("open pcm file err\n");
        return NULL;
    }
    printf("open pcm file: %s OK\n", pcm_file);
    return fp;
}

/**
 * print_playing_dot - print . when playing every second
 * notes:
 */
static void print_playing_dot(void)
{
    static unsigned char first_flag = 1;
    static struct ak_timeval cur_time;
    static struct ak_timeval print_time;

    if (first_flag) {
        first_flag = 0;
        ak_get_ostime(&cur_time);
        ak_get_ostime(&print_time);
        ak_print_normal(MODULE_ID_AO, "\n.");
    }

    ak_get_ostime(&cur_time);
    if (ak_diff_ms_time(&cur_time, &print_time) >= 1000) {
        ak_get_ostime(&print_time);
        ak_print_normal(MODULE_ID_AO, ".");
    }
}

/**
 * write_da_pcm - 
 * notes:
 */
static void write_da_pcm(int handle_id, FILE *fp, unsigned int channel_num,
                unsigned int volume)
{
    int read_len = 0;
    int send_len = 0;
    int total_len = 0;
    unsigned char data[PCM_READ_LEN] = {0};

    ak_ao_set_speaker(handle_id, AUDIO_FUNC_ENABLE);

    ak_ao_set_gain(handle_id, 6);
    ak_ao_set_volume(handle_id, volume);

    ak_ao_clear_frame_buffer(handle_id);

    ak_ao_set_dev_buf_size(handle_id, AK_AUDIO_DEV_BUF_SIZE_4096);
    
    while(1) 
    {
        memset(data, 0x00, sizeof(data));
        read_len = fread(data, sizeof(char), sizeof(data), fp);

        if(read_len > 0) 
        {
            /* send frame and play */
            if (ak_ao_send_frame(handle_id, data, read_len, &send_len)) 
            {
                ak_print_error_ex(MODULE_ID_AO, "write pcm to DA error!\n");
                break;
            }
            total_len += send_len;

            print_playing_dot();
            ak_sleep_ms(10);
        } else if(0 == read_len) {
            ak_ao_wait_play_finish(handle_id);
            /* read to the end of file */
            ak_print_normal(MODULE_ID_AO, "\n\t read to the end of file\n");

            break;
        } else {
            ak_print_error(MODULE_ID_AO, "read, %s\n", strerror(errno));
            break;
        }

    }

    /* disable speaker */
    ak_ao_set_speaker(handle_id, AUDIO_FUNC_DISABLE);
    ak_print_normal(MODULE_ID_AO, "%s exit\n", __func__);
}

/*
 * help_hint: use the -h --help option.Print option of help information
 * return: 0
 */
static int help_hint( char *pc_prog_name )
{
    int i;

    printf("%s\n" , pc_prog_name);
    for(i = 0; i < sizeof(option_long) / sizeof(struct option); i ++) {
        printf("\t--%-16s -%c %s\n" , option_long[ i ].name , option_long[ i ].val , ac_option_hint[ i ]);
    }
    printf("\n\n");
    return 0;
}

/**
 * get_option_short - 
 * notes:
 */
char *get_option_short( struct option *p_option, int i_num_option, char *pc_option_short, int i_len_option )
{
    int i;
    int i_offset = 0;
    char c_option;

    for( i = 0 ; i < i_num_option ; i ++ ) {
        c_option = p_option[ i ].val;
        switch( p_option[ i ].has_arg ){
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
    while((i_option = getopt_long(argc , argv , ac_option_short , option_long , NULL)) > 0) {
        switch(i_option) {
            case 'h' :                                                          //help
                help_hint( argv[ 0 ] );
                c_flag = 0;
                goto parse_option_end;
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
            case 'p' :                                                          //pcm-file
                pcm_file = optarg;
                break;
        }
    }
parse_option_end:
    return c_flag;
}

/**
 * Preconditions:
 * 1. T card is already mounted
 * 2. speaker must ready
 */
int main(int argc, char **argv)
{
    /* start the application */
    sdk_run_config config;
    config.mem_trace_flag = SDK_RUN_DEBUG;
    ak_sdk_init( &config );

    ak_print_normal(MODULE_ID_AO, "*****************************************\n");
	ak_print_normal(MODULE_ID_AO, "** ao demo version: %s **\n", ak_ao_get_version());
    ak_print_normal(MODULE_ID_AO, "*****************************************\n");
	
    if( parse_option( argc, argv ) == 0 ) 
    {
		ak_print_error_ex(MODULE_ID_AO,"parse param invalid!\n");
		return 0;
    }

    FILE *fp = NULL;
    printf("default play path: /mnt/ak_ao_test.pcm\n");

    fp = open_pcm_file(pcm_file);
    if(NULL == fp) {
        printf("open file error\n");
        return -1;
    }

    int ret = 0;
        
    struct ak_audio_out_param ao_param;
    ao_param.pcm_data_attr.sample_bits = 16;
    ao_param.pcm_data_attr.channel_num = channel_num;
    ao_param.pcm_data_attr.sample_rate = sample_rate;
    ao_param.dev_id = 0;

    /* open ao */
    int ao_handle_id = -1;
    if(ak_ao_open(&ao_param, &ao_handle_id)) {
        ret = -1;
        goto main_end;
    }

    /* get pcm data,send to play */
    write_da_pcm(ao_handle_id, fp, channel_num, volume);

    /* play finish,close pcm file */
    if(NULL != fp) {
        fclose(fp);
        fp = NULL;
    }

    /* close ao */
    ak_ao_close(ao_handle_id);
    ao_handle_id = -1;

main_end:
    printf("----- %s exit -----\n", argv[0]);

    return ret;
}
