#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "ak_ao.h"
#include "ak_common.h"
#include "ak_log.h"
#include "ak_mem.h"

#define PCM_READ_LEN 4096
#define AO_DAC_MAX_VOLUME 12 /* max volume according to ao */
#define TEST_AO_VOLUME 0     /* test audio volume */
#define LEN_HINT 512
#define LEN_OPTION_SHORT 512

// int sample_rate = 8000; //8000 12000 11025 16000 22050 24000 32000 44100 48000
// int volume = 3;
char *audio_frame_file = "/mnt/frame/audio_frame.pcm";
char *bell_pcm_file = "/mnt/bell.pcm";
int ao_handle_id = -1;
int play_bell_flag = 0;

// int channel_num = AUDIO_CHANNEL_MONO;

//用于放语音
void read_pcm_routine(void *arg)
{
        FILE *fp_pcm_o = NULL;
        unsigned int volume = 3;
        int read_len = 0;
        int send_len = 0;
        int total_len = 0;
        unsigned char data[4096] = {0};

        char *pcmfilename = (char *)arg;
        // printf("pcm filename = %s\n", pcmfilename);

        fp_pcm_o = fopen(pcmfilename, "r");
        if (NULL == fp_pcm_o)
        {
                printf("open file error\n");
                return -1;
        }

        ak_ao_set_speaker(ao_handle_id, 1); //AUDIO_FUNC_ENABLE
        ak_ao_set_gain(ao_handle_id, 6);
        ak_ao_set_volume(ao_handle_id, volume);
        ak_ao_clear_frame_buffer(ao_handle_id);
        ak_ao_set_dev_buf_size(ao_handle_id, 4096); //AK_AUDIO_DEV_BUF_SIZE_4096

        while (1)
        {
                memset(data, 0x00, sizeof(data));
                read_len = fread(data, sizeof(char), sizeof(data), fp_pcm_o);

                if (read_len > 0)
                {
                        /* send frame and play */
                        if (ak_ao_send_frame(ao_handle_id, data, read_len, &send_len))
                        {
                                printf("write pcm to DA error!\n");
                                break;
                        }
                        total_len += send_len;

                        // print_playing_dot();
                        ak_sleep_ms(10);
                }
                else if (0 == read_len)
                {
                        ak_ao_wait_play_finish(ao_handle_id);
                        printf("read to the end of file\n");

                        break;
                }
                else
                {
                        printf("read error\n");
                        break;
                }
        }
        fclose(fp_pcm_o);
        /* disable speaker */
        // ak_ao_set_speaker(ao_handle_id, 0);
        // ak_ao_close(ao_handle_id);
        // ao_handle_id = -1;
}

//用于放铃声
void read_pcm( char *pcmfilename)
{
        FILE *fp_pcm_o = NULL;
        unsigned int volume = 3;
        int read_len = 0;
        int send_len = 0;
        int total_len = 0;
        unsigned char data[4096] = {0};

        fp_pcm_o = fopen(pcmfilename, "r");
        if (NULL == fp_pcm_o)
        {
                printf("open file error\n");
                return -1;
        }

        ak_ao_set_speaker(ao_handle_id, 1); //AUDIO_FUNC_ENABLE
        ak_ao_set_gain(ao_handle_id, 6);
        ak_ao_set_volume(ao_handle_id, volume);
        ak_ao_clear_frame_buffer(ao_handle_id);
        ak_ao_set_dev_buf_size(ao_handle_id, 4096); //AK_AUDIO_DEV_BUF_SIZE_4096

        while (1)
        {
                memset(data, 0x00, sizeof(data));
                read_len = fread(data, sizeof(char), sizeof(data), fp_pcm_o);

                if (read_len > 0)
                {
                        /* send frame and play */
                        if (ak_ao_send_frame(ao_handle_id, data, read_len, &send_len))
                        {
                                printf("write pcm to DA error!\n");
                                break;
                        }
                        total_len += send_len;

                        // print_playing_dot();
                        ak_sleep_ms(10);
                }
                else if (0 == read_len)
                {
                        ak_ao_wait_play_finish(ao_handle_id);
                        printf("read to the end of file\n");

                        break;
                }
                else
                {
                        printf("read error\n");
                        break;
                }
        }
        fclose(fp_pcm_o);
        /* disable speaker */
        // ak_ao_set_speaker(ao_handle_id, 0);
        // ak_ao_close(ao_handle_id);
        // ao_handle_id = -1;
}

void play_bell_routine()
{
        while (play_bell_flag)
        {
                read_pcm(bell_pcm_file);
        }
}

int ak_ao_init()
{
        /* start the application */
        sdk_run_config config;
        int channel_num = 1;
        int sample_rate = 8000;
        config.mem_trace_flag = SDK_RUN_DEBUG;
        ak_sdk_init(&config);

        // FILE *fp_pcm_o = NULL;
        printf("default play path: /mnt/frame/ak_ao_test.pcm\n");

        struct ak_audio_out_param ao_param;
        ao_param.pcm_data_attr.sample_bits = 16;
        ao_param.pcm_data_attr.channel_num = channel_num;
        ao_param.pcm_data_attr.sample_rate = sample_rate;
        ao_param.dev_id = 0;

        /* open ao */
        // int ao_handle_id = -1;
        if (ak_ao_open(&ao_param, &ao_handle_id))
        {
                printf("ao open error\n");
                return;
        }
}
