#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include "tslib.h"
#include "fbutils.h"
#include "testutils.h"
#include "font_big.h"

#include "ak_common.h"
#include "ak_common_graphics.h"
#include "ak_vi.h"
#include "ak_ai.h"
#include "ak_ao.h"
#include "ak_mem.h"
#include "ak_log.h"
#include "ak_venc.h"

#define STATE_WELCOME 0
#define STATE_CALL 1
#define STATE_NUM_ERROR 2
#define FRAME_WIDTH 640  //584, 640
#define FRAME_HEIGHT 480 //600, 480
// #define XRES 1024
// #define YRES 600

#define NR_COLORS (sizeof(palette) / sizeof(palette[0]))
#define NR_BUTTONS 5 //按钮数量
#define TEXTBOXES_NUM 1

//调色板，存储需要写入colormap的颜色，BGR格式
static int palette[] = {
    0xD5D5D5,           //背景色0、按钮边框色0
    0x000000,           //字体色黑色默认1
    0xF9F9F9, 0xC0C0C0, // 数字键未激活2、激活色3
    0xEEEEEE, 0xC0C0C0, //其他键未激活4、激活色5
    0x00CC00, 0x00AA00, //绿色未激活6、绿色激活7
    0x0033F9, 0x0022C9  //红色未激活8、红色激活9
};

int current_state = STATE_WELCOME;           //当前输入状态
static struct ts_button buttons[NR_BUTTONS]; //按钮数组
struct timeval start;

extern int ai_capture_enable; //录音持续使能
extern int vi_capture_enable; //摄像持续使能
extern int play_bell_flag;    //播放铃声使能
extern int ai_handle_id;
extern int ao_handle_id;

static void sig(int sig)
{
        close_framebuffer();
        fflush(stderr);
        printf("signal %d caught\n", sig);
        fflush(stdout);
        exit(1);
}

void print_usage_info()
{
        fillrect(0, 200, xres / 7 * 4, 400, 0); //信息背景
        switch (current_state)
        {
        case STATE_WELCOME:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_input_number, 6, 48, 1);
                put_const_string_center(xres / 7 * 2, 340, CS_48x48_press_to_call, 6, 48, 1);
                break;
        case STATE_CALL:
                put_const_string_center(xres / 7 * 2 - 40, 250, CS_48x48_calling, 4, 48, 1);
                put_const_string_center(xres / 7 * 2, 340, CS_48x48_cancel, 6, 48, 1);
                break;
        case STATE_NUM_ERROR:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_wrong_number, 5, 48, 1);
                put_const_string_center(xres / 7 * 2, 340, CS_48x48_retry, 3, 48, 1);
                break;

        default:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_input_number, 6, 48, 1);
                put_const_string_center(xres / 7 * 2, 340, CS_48x48_press_to_call, 6, 48, 1);
                break;
        }
}

static void refresh_screen(void)
{
        int i;

        fillrect(0, 0, xres - 1, yres - 1, 0); //背景
        put_const_string_center(xres / 8 * 3, 60, CS_48x48_sysname, 8, 48, 1);
        // print_usage_info();

        for (i = 0; i < NR_BUTTONS; i++)
                button_draw(&buttons[i]);
}

void init_widget()
{
        int i, j;

        /* Initialize buttons */
        int btn_w = xres / 8;
        int btn_h = yres / 5;

        memset(&buttons, 0, sizeof(buttons));
        for (i = 0; i < NR_BUTTONS; i++)
        {
                buttons[i].w = btn_w;
                buttons[i].h = btn_h;
                buttons[i].x = btn_w * 7;
                buttons[i].fill_colidx[0] = 2;
                buttons[i].fill_colidx[1] = 3;
                buttons[i].border_colidx[0] = buttons[i].border_colidx[1] = 0;
                buttons[i].font_colidx[0] = buttons[i].font_colidx[1] = 1;
        }

        buttons[0].text = "["; //接听
        buttons[1].text = "]"; //挂断
        buttons[2].text = "&"; //语音
        buttons[3].text = "$"; //锁
        buttons[4].text = "@"; //摄像头

        buttons[0].font_colidx[0] = buttons[0].font_colidx[1] = 2;
        buttons[1].font_colidx[0] = buttons[1].font_colidx[1] = 2;

        buttons[0].fill_colidx[0] = 6; //接听未激活
        buttons[0].fill_colidx[1] = 7; //接听激活
        buttons[1].fill_colidx[0] = 8; //挂断未激活
        buttons[1].fill_colidx[1] = 9; //挂断激活

        buttons[0].y = btn_h * 3;
        buttons[1].y = btn_h * 4;
        buttons[2].y = btn_h * 2;
        buttons[3].y = 0;
        buttons[4].y = btn_h;

        refresh_screen();
}

void print_time()
{
        struct timeval end;
        static struct timeval last;

        int timespend;

        gettimeofday(&end, NULL);
        printf("wating time = %d\n", end.tv_sec - start.tv_sec);
}

void waiting_dots(void)
{
        int i, j;
        while (1)
        {
                sleep(1);
                for (i = 0; i < 3; i++)
                {
                        put_const_string(350 + i * 32, 226, fontdata_48x48, 1, 48, 1);
                        //print_time();
                        sleep(1);
                }
                for (i = 0; i < 3; i++)
                {
                        put_const_string(350 + i * 32, 226, fontdata_48x48, 1, 48, 0);
                }
        }
}

void open_yuv(char *filename)
{
        unsigned char *rgb_p;
        char *YUVmap;
        uint32_t *RGBmap;
        int i, j;

        YUVmap = (char *)malloc(sizeof(char) * FRAME_WIDTH * FRAME_HEIGHT * 3);
        FILE *yuv_fp = fopen(filename, "r");
        if (yuv_fp == NULL)
        {
                printf("yuv file not exist!");
                return;
        }
        fread(YUVmap, 1, FRAME_WIDTH * FRAME_HEIGHT * 3, yuv_fp);
        printf("yuv data get\n");
        // printf("yuv data:\n");
        // for (i = 0; i < FRAME_WIDTH; i++)
        // {
        //         for (j = 0; j < FRAME_HEIGHT; j++)
        //         {
        //                 printf("%X  ", YUVmap[i * FRAME_WIDTH + j]);
        //         }
        //         printf("\n\n");
        // }
        //RGBmap = (char *)malloc(sizeof(uint32_t) * FRAME_WIDTH * FRAME_HEIGHT);
        RGBmap = yuv420_to_rgb(YUVmap, FRAME_WIDTH, FRAME_HEIGHT);
        if (RGBmap == NULL)
        {
                printf("RGBmap is NULL!\n");
                return;
        }
        put_rgb_map(0, 0, RGBmap, FRAME_WIDTH, FRAME_HEIGHT);
        free(YUVmap);
}

int main(int argc, char **argv)
{
        struct tsdev *ts;
        // int x, y;
        unsigned int i;
        unsigned int mode = 0;
        pthread_t vi_thread, ai_thread, bell_thread;

        signal(SIGSEGV, sig); //这三个是程序中断退出信号
        signal(SIGINT, sig);  //按Ctrl+c退出程序
        signal(SIGTERM, sig); //kill(1)终止

        ts = ts_setup(NULL, 0);
        if (!ts)
        {
                perror("ts_open");
                exit(1);
        }

        if (open_framebuffer())
        {
                close_framebuffer();
                ts_close(ts);
                exit(1);
        }

        // x = xres / 2;
        // y = yres / 2;

        for (i = 0; i < NR_COLORS; i++)
                setcolor(i, palette[i]);

        init_widget();
        ai_handle_id = ak_ai_init();
        ao_handle_id = ak_ao_init();
        ak_vi_init();

        while (1)
        {
                struct ts_sample samp;
                int ret;

                /* Show the cross */
                // if ((mode & 15) != 1)
                //         put_cross(x, y, 4 | XORMODE);

                ret = ts_read(ts, &samp, 1);
                samp.x = xres - samp.x;

                /* Hide it */
                // if ((mode & 15) != 1)
                //         put_cross(x, y, 4 | XORMODE);

                if (ret < 0)
                {
                        perror("ts_read");
                        close_framebuffer();
                        ts_close(ts);
                        exit(1);
                }

                if (ret != 1)
                        continue;

                //按钮事件
                for (i = 0; i < NR_BUTTONS; i++)
                {
                        if (button_handle(&buttons[i], samp.x, samp.y, samp.pressure))
                        {
                                switch (i)
                                {
                                case 0: //[
                                {
                                        if (current_state == STATE_WELCOME)
                                        {
                                                current_state = STATE_CALL;
                                                // vi_capture_enable = 1;
                                                // pthread_create(&vi_thread, NULL, (void *)vi_capture_loop, NULL);

                                                // play_bell_flag = 1;
                                                // pthread_create(&bell_thread, NULL, (void *)play_bell_routine, NULL);
                                        }
                                        else if (current_state == STATE_CALL)
                                        {
                                                current_state = STATE_WELCOME;
                                                vi_capture_enable = 0;
                                                play_bell_flag = 0;
                                                refresh_screen();
                                        }
                                }
                                break;
                                case 1: //]
                                {
                                        current_state = STATE_WELCOME;
                                        vi_capture_enable = 0;
                                        play_bell_flag = 0;
                                        refresh_screen();
                                }
                                break;
                                case 2: //&语音
                                {
                                        if (ai_capture_enable == 0)
                                        {
                                                ai_capture_enable = 1;
                                                pthread_create(&ai_thread, NULL, (void *)ai_capture_loop, &ai_handle_id);
                                        }
                                        else if (ai_capture_enable == 1)
                                        {
                                                ai_capture_enable = 0;
                                                read_pcm(3, "/mnt/frame/audio_frame.pcm");
                                        }
                                }
                                break;
                                case 4: //@摄像头
                                {
                                        if (vi_capture_enable == 0)
                                        {
                                                vi_capture_enable = 1;
                                                pthread_create(&vi_thread, NULL, (void *)vi_capture_loop, NULL);
                                        }
                                        else if (vi_capture_enable == 1)
                                        {
                                                vi_capture_enable = 0;
                                                refresh_screen();
                                        }
                                }
                                break;
                                default:
                                        break;
                                }
                        }
                }

                //printf("%ld.%06ld: %6d %6d %6d\n", samp.tv.tv_sec, samp.tv.tv_usec, samp.x, samp.y, samp.pressure);
                // if (samp.pressure > 0)
                // {
                //         if (mode == 0x80000001)
                //                 line(x, y, samp.x, samp.y, 2);
                //         x = samp.x;
                //         y = samp.y;
                //         mode |= 0x80000000;
                // }
                // else
                //         mode &= ~0x80000000;
        }
        fillrect(0, 0, xres - 1, yres - 1, 0);
        close_framebuffer();
        ts_close(ts);

        return 0;
}