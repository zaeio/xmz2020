#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include "ak_common.h"
#include "ak_common_graphics.h"
#include "ak_mem.h"
#include "ak_log.h"
#include "ak_ai.h"
#include "ak_ao.h"
#include "ak_vi.h"

#include "fbutils.h"

int ai_handle_id = -1;
int ao_handle_id = -1;

void audio_get_frame()
{
        int send_len = 0;
        struct frame frame = {0};
        int i;

        ak_ao_set_speaker(ao_handle_id, 1); //AUDIO_FUNC_ENABLE
        ak_ao_set_gain(ao_handle_id, 3);
        ak_ao_set_volume(ao_handle_id, 3);
        ak_ao_clear_frame_buffer(ao_handle_id);
        ak_ao_set_dev_buf_size(ao_handle_id, 512); //AK_AUDIO_DEV_BUF_SIZE_4096

        while (1)
        {
                /* get the pcm data frame */
                int ret = ak_ai_get_frame(ai_handle_id, &frame, 0);
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
                        printf("!frame.data || frame.len <= 0");
                        continue;
                }

                if (ak_ao_send_frame(ao_handle_id, frame.data, 512, &send_len))
                {
                        ak_print_error_ex(MODULE_ID_AO, "write pcm to DA error!\n");
                        break;
                }
                // ak_sleep_ms(10);
                printf("frame_len= %d    send_len = %d\n", frame.len, send_len);
                ak_ai_release_frame(ai_handle_id, &frame);
        }
        ak_ao_set_speaker(ao_handle_id, 0); //AUDIO_FUNC_DISABLE
}

void ak_aio_init()
{
        ai_handle_id = ak_ai_init();
        ao_handle_id = ak_ao_init();
}