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
#include "av_socket.h"

#define STATE_WELCOME 0
#define STATE_CALL 1
#define STATE_ONLINE 2
#define STATE_NUM_ERROR 3

#define FRAME_WIDTH 896  //584, 640
#define FRAME_HEIGHT 600 //600, 480
// #define XRES 1024
// #define YRES 600

#define NR_COLORS (sizeof(palette) / sizeof(palette[0]))
#define NR_BUTTONS 12 //按钮数量
#define TEXTBOXES_NUM 1

//调色板，存储需要写入colormap的颜色，BGR格式
static int palette[] = {
    0xB5B5B5,           //背景色0、按钮边框色0
    0x000000,           //字体色1
    0xF9F9F9, 0xC0C0C0, // 数字键未激活2、激活色3
    0xEEEEEE, 0xC0C0C0, //其他键未激活4、激活色5
    0x00CC00, 0x00AA00, //绿色未激活6、绿色激活7
    0x0033F9, 0x0022C9  //红色未激活8、红色激活9
};

extern int outdoor_current_state;           //当前输入状态
static struct ts_button buttons[NR_BUTTONS]; //按钮数组
struct ts_textbox textboxes[TEXTBOXES_NUM];
struct timeval start;

pthread_t outdoor_vi_thread, outdoor_ai_thread;
extern pthread_t tcp_outdoor_thread;
extern int ai_capture_enable; //录音持续使能
extern int vi_capture_enable; //摄像持续使能
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
        switch (outdoor_current_state)
        {
        case STATE_WELCOME:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_input_number, 6, 48, 1);
                // put_const_string_center(xres / 7 * 2, 340, CS_48x48_press_to_call, 6, 48, 1);
                break;
        case STATE_CALL:
                put_const_string_center(xres / 7 * 2 - 40, 250, CS_48x48_calling, 4, 48, 1);
                // put_const_string_center(xres / 7 * 2, 340, CS_48x48_cancel, 6, 48, 1);
                break;
        case STATE_NUM_ERROR:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_wrong_number, 5, 48, 1);
                put_const_string_center(xres / 7 * 2, 340, CS_48x48_retry, 3, 48, 1);
                break;

        default:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_input_number, 6, 48, 1);
                // put_const_string_center(xres / 7 * 2, 340, CS_48x48_press_to_call, 6, 48, 1);
                break;
        }
}

static void refresh_screen(void)
{
        int i;

        fillrect(0, 0, xres - 1, yres - 1, 0); //背景
        put_const_string_center(xres / 7 * 2, 60, CS_48x48_sysname, 8, 48, 1);
        print_usage_info();

        for (i = 0; i < TEXTBOXES_NUM; i++)
                textbox_draw(&textboxes[0]);

        for (i = 0; i < NR_BUTTONS; i++)
                button_draw(&buttons[i]);
}

void init_widget()
{
        int i, j;

        /* Initialize buttons */
        int btn_number_w = xres / 7;
        int btn_number_h = yres / 5;

        memset(&buttons, 0, sizeof(buttons));
        for (i = 0; i < NR_BUTTONS; i++)
        {
                buttons[i].w = btn_number_w;
                buttons[i].h = btn_number_h;
                buttons[i].fill_colidx[0] = 2;
                buttons[i].fill_colidx[1] = 3;
                buttons[i].border_colidx[0] = buttons[i].border_colidx[1] = 0;
                buttons[i].font_colidx[0] = buttons[i].font_colidx[1] = 1;
        }

        buttons[10].fill_colidx[0] = 6; //接听
        buttons[10].fill_colidx[1] = 7;
        buttons[10].font_colidx[0] = buttons[10].font_colidx[1] = 2;
        buttons[11].fill_colidx[0] = 4; //*的颜色
        buttons[11].fill_colidx[1] = 5; //*的颜色

        buttons[10].x = buttons[1].x = buttons[4].x = buttons[7].x = btn_number_w * 4;
        buttons[0].x = buttons[2].x = buttons[5].x = buttons[8].x = btn_number_w * 5;
        buttons[11].x = buttons[3].x = buttons[6].x = buttons[9].x = btn_number_w * 6;

        buttons[7].y = buttons[8].y = buttons[9].y = btn_number_h;
        buttons[4].y = buttons[5].y = buttons[6].y = btn_number_h * 2;
        buttons[1].y = buttons[2].y = buttons[3].y = btn_number_h * 3;
        buttons[10].y = buttons[0].y = buttons[11].y = btn_number_h * 4;

        buttons[0].text = "0";
        buttons[1].text = "1";
        buttons[2].text = "2";
        buttons[3].text = "3";
        buttons[4].text = "4";
        buttons[5].text = "5";
        buttons[6].text = "6";
        buttons[7].text = "7";
        buttons[8].text = "8";
        buttons[9].text = "9";
        buttons[10].text = "[";
        buttons[11].text = "*";

        /* Initialize textboxes */
        textboxes[0].x = xres / 7 * 4;
        textboxes[0].y = 0;
        textboxes[0].w = xres / 7 * 3;
        textboxes[0].h = yres / 5;
        textboxes[0].fill_colidx = 4; //4
        textboxes[0].border_colidx = 0;

        for (j = 0; j < TEXTBOXES_NUM; j++)
        {
                textboxes[j].text_cap = 4;
                textboxes[j].font_colidx = 1;
                for (i = 0; i <= textboxes[j].text_cap; i++, textboxes[j].text[i] = '\0')
                        ;
        }

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
        setup_client_tcp();

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
                                case 10: //#
                                {
                                        if (outdoor_current_state == STATE_WELCOME)
                                        {
                                                int floor, num;
                                                floor = (textboxes[0].text[0] - '0') * 10 + (textboxes[0].text[1] - '0');
                                                num = (textboxes[0].text[2] - '0') * 10 + (textboxes[0].text[3] - '0');

                                                if (floor > 0 && floor <= 20 && num > 0 && num <= 9)
                                                {
                                                        outdoor_current_state = STATE_CALL;
                                                        print_usage_info();

                                                        //将#修改为挂断
                                                        buttons[10].fill_colidx[0] = 8;
                                                        buttons[10].fill_colidx[1] = 9;
                                                        buttons[10].text = "]";
                                                        button_draw(&buttons[10]);

                                                        // if (pthread_create(&thID_dot, NULL, (void *)waiting_dots, NULL) != 0)
                                                        // {
                                                        //         printf("Create pthread error!\n");
                                                        //         exit(1);
                                                        // }

                                                        // open_yuv("/mnt/frame/test.yuv");

                                                        // vi_capture_enable = 1;
                                                        // pthread_create(&outdoor_vi_thread, NULL, (void *)vi_capture_loop, NULL);

                                                        // ai_capture_enable = 1;
                                                        // pthread_create(&ai_thread, NULL, (void *)ai_capture_loop, &ai_handle_id);

                                                        send_call();
                                                        pthread_create(&tcp_outdoor_thread, NULL, (void *)receive_outdoor_routine, NULL);
                                                        // send_Vframe();
                                                }
                                                else
                                                {
                                                        outdoor_current_state = STATE_NUM_ERROR;
                                                        print_usage_info();
                                                }
                                        }
                                        else if (outdoor_current_state == STATE_CALL || outdoor_current_state == STATE_ONLINE)
                                        {
                                                vi_capture_enable = 0;
                                                ai_capture_enable = 0;
                                                // pthread_cancel(outdoor_vi_thread);
                                                // pthread_cancel(ai_thread);

                                                outdoor_current_state = STATE_WELCOME;
                                                //将#修改为接听
                                                buttons[10].fill_colidx[0] = 6;
                                                buttons[10].fill_colidx[1] = 7;
                                                buttons[10].text = "[";
                                                button_draw(&buttons[10]);

                                                // refresh_screen();
                                                print_usage_info();
                                        }
                                }
                                break;
                                case 11: //删除
                                        textbox_delchar(&textboxes[0]);
                                        if (outdoor_current_state == STATE_NUM_ERROR)
                                        {
                                                outdoor_current_state = STATE_WELCOME;
                                                print_usage_info();
                                        }
                                        break;
                                default: //数字键
                                        textbox_addchar(&textboxes[0], '0' + i);
                                        if (outdoor_current_state == STATE_NUM_ERROR)
                                        {
                                                outdoor_current_state = STATE_WELCOME;
                                                print_usage_info();
                                        }
                                        break;
                                }
                        }
                }
        }
        fillrect(0, 0, xres - 1, yres - 1, 0);
        close_framebuffer();
        ts_close(ts);

        return 0;
}
