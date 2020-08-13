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
char *pcm_file = "/mnt/frame/ak_ao_test.pcm";
// int channel_num = AUDIO_CHANNEL_MONO;

static FILE *open_pcm_file(const char *pcm_file)
{
        FILE *fp = fopen(pcm_file, "r");
        if (NULL == fp)
        {
                printf("open pcm file err\n");
                return NULL;
        }
        printf("open pcm file: %s OK\n", pcm_file);
        return fp;
}


static void read_pcm(int handle_id, FILE *fp, unsigned int volume)
{
        int read_len = 0;
        int send_len = 0;
        int total_len = 0;
        unsigned char data[4096] = {0};

        ak_ao_set_speaker(handle_id, 1); //AUDIO_FUNC_ENABLE
        ak_ao_set_gain(handle_id, 6);
        ak_ao_set_volume(handle_id, volume);
        ak_ao_clear_frame_buffer(handle_id);
        ak_ao_set_dev_buf_size(handle_id, 4096); //AK_AUDIO_DEV_BUF_SIZE_4096

        while (1)
        {
                memset(data, 0x00, sizeof(data));
                read_len = fread(data, sizeof(char), sizeof(data), fp);

                if (read_len > 0)
                {
                        /* send frame and play */
                        if (ak_ao_send_frame(handle_id, data, read_len, &send_len))
                        {
                                printf("write pcm to DA error!\n");
                                break;
                        }
                        total_len += send_len;

                        print_playing_dot();
                        ak_sleep_ms(10);
                }
                else if (0 == read_len)
                {
                        ak_ao_wait_play_finish(handle_id);
                        printf("read to the end of file\n");

                        break;
                }
                else
                {
                        printf("read error\n");
                        break;
                }
        }
        /* disable speaker */
        ak_ao_set_speaker(handle_id, 0);
}

int ak_ao_init()
{
        /* start the application */
        sdk_run_config config;
        int channel_num = 1;
        int sample_rate = 8000;
        config.mem_trace_flag = SDK_RUN_DEBUG;
        ak_sdk_init(&config);

        FILE *fp = NULL;
        printf("default play path: /mnt/frame/ak_ao_test.pcm\n");

        fp = open_pcm_file(pcm_file);
        if (NULL == fp)
        {
                printf("open file error\n");
                return -1;
        }

        struct ak_audio_out_param ao_param;
        ao_param.pcm_data_attr.sample_bits = 16;
        ao_param.pcm_data_attr.channel_num = channel_num;
        ao_param.pcm_data_attr.sample_rate = sample_rate;
        ao_param.dev_id = 0;

        /* open ao */
        int ao_handle_id = -1;
        if (ak_ao_open(&ao_param, &ao_handle_id))
        {
                printf("ao open error\n");
                return;
        }

        /* get pcm data,send to play */
        //         write_da_pcm(ao_handle_id, fp, channel_num, volume);

        //         /* play finish,close pcm file */
        //         if (NULL != fp)
        //         {
        //                 fclose(fp);
        //                 fp = NULL;
        //         }

        //         /* close ao */
        //         ak_ao_close(ao_handle_id);
        //         ao_handle_id = -1;

        return ao_handle_id;
}
