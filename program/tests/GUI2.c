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
#define STATE_CALLED 1
#define STATE_ONLINE 2

#define FRAME_WIDTH 896  //584, 640
#define FRAME_HEIGHT 600 //600, 480
// #define XRES 1024
// #define YRES 600

#define NR_COLORS (sizeof(palette) / sizeof(palette[0]))
#define NR_BUTTONS 5 //按钮数量
#define TEXTBOXES_NUM 1

//调色板，存储需要写入colormap的颜色，BGR格式
static int palette[] = {
    0xB5B5B5,           //背景色0、按钮边框色0     0xD5D5D5
    0x000000,           //字体色黑色默认1
    0xF9F9F9, 0xC0C0C0, // 数字键未激活2、激活色3
    0xEEEEEE, 0xC0C0C0, //其他键未激活4、激活色5
    0x00CC00, 0x00AA00, //绿色未激活6、绿色激活7
    0x0033F9, 0x0022C9  //红色未激活8、红色激活9
};

int indoor_current_state = STATE_WELCOME;    //当前输入状态
static struct ts_button buttons[NR_BUTTONS]; //按钮数组
struct timeval start;

extern int server_fd, connfd;
pthread_t indoor_vi_thread, indoor_vo_thread, indoor_ai_thread, indoor_ao_thread;
pthread_t indoor_bell_thread, tcp_indoor_thread;
extern int ai_capture_enable; //录音持续使能
extern int vi_capture_enable; //摄像持续使能
extern int play_bell_flag;    //播放铃声使能
char monitor_enable = 0;
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
        switch (indoor_current_state)
        {
        case STATE_WELCOME:
                put_const_string_center(xres / 7 * 2, 250, CS_48x48_input_number, 6, 48, 1);
                put_const_string_center(xres / 7 * 2, 340, CS_48x48_press_to_call, 6, 48, 1);
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
        int i;

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
        // static struct timeval last;

        // int timespend;

        gettimeofday(&end, NULL);
        printf("wating time = %d\n", (int)(end.tv_sec - start.tv_sec));
}

void waiting_dots(void)
{
        int i;
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

void put_gray_map_routine(void *arg)
{
        char *graymap = (char *)arg;
        put_gray_map(0, 0, graymap, FRAME_WIDTH, FRAME_HEIGHT);
}

void receive_indoor_routine()
{
        char buf[896] = {0};
        char *graymap;

        printf("receiving\n");
        int recvlen = 0;
        int framecount = 0; //帧数
        int recvcount = 0;  //文件长度
        while ((recvlen = recv(connfd, buf, 896, 0)) > 0)
        {
                char file_name[255];
                // printf("recvlen = %d\n", recvlen);
                switch (get_header(buf, recvlen))
                {
                case BUFF_CMD_IMG: //frame
                {
                        // printf("BUFF_CMD_IMG\n");
                        //先结束上一帧
                        if (framecount > 0)
                        {
                                put_gray_map(0, 0, graymap, FRAME_WIDTH, FRAME_HEIGHT);
                                free(graymap);
                                recvcount = 0;
                        }
                        framecount++;
                        graymap = (char *)calloc(FRAME_WIDTH * FRAME_HEIGHT, sizeof(char));
                }
                break;
                case BUFF_CMD_CALL:
                {
                        printf("BUFF_CMD_CALL\n");
                        indoor_current_state = STATE_CALLED;
                        play_bell_flag = 1;
                        pthread_create(&indoor_bell_thread, NULL, (void *)play_bell_routine, NULL);
                }
                break;
                case BUFF_CMD_HANGUP:
                {
                        printf("BUFF_CMD_HANGUP\n");
                        play_bell_flag = 0;
                        usleep(200000);
                        refresh_screen();
                        // pthread_cancel(indoor_bell_thread);
                        indoor_current_state = STATE_WELCOME;
                }
                break;
                case BUFF_CMD_AUDIO:
                {
                        printf("BUFF_CMD_AUDIO\n");
                        char *outdoor_pcm = "/mnt/frame/outdoor.pcm";
                        usleep(200000);
                        pthread_create(&indoor_ao_thread, NULL, (void *)read_pcm_routine, outdoor_pcm);
                }
                break;
                default:
                {
                        //写入graymap
                        int i;
                        if (graymap != NULL)
                        {
                                for (i = 0; i < recvlen; i++)
                                {
                                        if (recvcount + i < FRAME_WIDTH * FRAME_HEIGHT)
                                                graymap[recvcount + i] = buf[i];
                                }
                                recvcount += recvlen;
                        }
                }
                break;
                }
        }
        printf("receive finished\n");
        close(server_fd);
        close(connfd);

        //退出线程
        pthread_exit(NULL);
}

//=======================================================================================
int main(int argc, char **argv)
{
        struct tsdev *ts;
        // int x, y;
        unsigned int i;
        // unsigned int mode = 0;

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

        if (setup_server_tcp())
                pthread_create(&tcp_indoor_thread, NULL, (void *)receive_indoor_routine, NULL);

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
                                case 0: //[接听
                                {
                                        if (indoor_current_state == STATE_CALLED)
                                        {
                                                indoor_current_state = STATE_ONLINE;
                                                play_bell_flag = 0;
                                                // pthread_cancel(indoor_bell_thread);
                                                send_answer();

                                                // play_bell_flag = 1;
                                                // pthread_create(&indoor_bell_thread, NULL, (void *)play_bell_routine, NULL);
                                        }
                                }
                                break;
                                case 1: //]挂断
                                {
                                        if (indoor_current_state != STATE_WELCOME)
                                        {
                                                indoor_current_state = STATE_WELCOME;
                                                play_bell_flag = 0;
                                                send_hangup(1);
                                                usleep(200000);
                                                refresh_screen();
                                                // pthread_cancel(indoor_bell_thread);
                                                
                                        }
                                }
                                break;
                                case 2: //&语音
                                {
                                        if (ai_capture_enable == 0)
                                        {
                                                ai_capture_enable = 1;
                                                char *indoor_pcm = "/mnt/frame/indoor.pcm";
                                                pthread_create(&indoor_ai_thread, NULL, (void *)ai_capture_loop, indoor_pcm);
                                        }
                                        else if (ai_capture_enable == 1)
                                        {
                                                ai_capture_enable = 0;
                                                send_audio(1);
                                                // char *intdoor_pcm = "/mnt/frame/indoor.pcm";
                                                // pthread_create(&indoor_ao_thread, NULL, (void *)read_pcm_routine, intdoor_pcm);
                                        }
                                }
                                break;
                                case 3:
                                        send_unlock();
                                        break;
                                case 4: //@摄像头
                                {
                                        if (indoor_current_state == STATE_WELCOME)
                                        {
                                                if (monitor_enable == 0)
                                                {
                                                        send_camera(1);
                                                        monitor_enable = 1;
                                                        // vi_capture_enable = 1;
                                                        // pthread_create(&indoor_vi_thread, NULL, (void *)vi_capture_loop, NULL);
                                                }
                                                else if (monitor_enable == 1)
                                                {
                                                        send_camera(0);
                                                        monitor_enable = 0;
                                                        usleep(200000);
                                                        refresh_screen();
                                                }
                                        }
                                }
                                break;
                                default:
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
