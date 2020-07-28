#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>

#include "tslib.h"
#include "fbutils.h"
#include "testutils.h"

// #define XRES 1024
// #define YRES 600
char input_numbers[5] = {'\0', '\0', '\0', '\0', '\0'}; //输入数字缓存
//调色板，存储需要写入colormap的颜色，BGR格式
static int palette[] = {
    0xD5D5D5,            //背景色0、按钮边框色0
    0x000000,            //字体色1
    0xF9F9F9, 0xC0C0C0,  // 数字键未激活2、激活色3
    0xEEEEEE, 0xC0C0C0}; //其他键未激活4、激活色5

#define NR_COLORS (sizeof(palette) / sizeof(palette[0]))
#define NR_BUTTONS 12                        //按钮数量
static struct ts_button buttons[NR_BUTTONS]; //按钮数组

static void sig(int sig)
{
    close_framebuffer();
    fflush(stderr);
    printf("signal %d caught\n", sig);
    fflush(stdout);
    exit(1);
}

static void refresh_screen(void)
{
    int i;

    fillrect(0, 0, xres - 1, yres - 1, 0);                    //背景
    fillrect(xres / 7 * 4 + 2, 2, xres - 2, yres / 5 - 2, 4); //数字输入框
    //put_string_center(xres / 2, yres / 4, "239", 1);

    for (i = 0; i < NR_BUTTONS; i++)
        button_draw(&buttons[i]);
    refresh_number();
}

void refresh_number()
{
    fillrect(xres / 7 * 4 + 2, 2, xres - 2, yres / 5 - 2, 4); //数字输入框
    put_string_center((int)(xres / 7 * 5.5), yres / 8, input_numbers, 1);
}
void input_number(int num)
{
    int i;
    for (i = 0; i < 4 && input_numbers[i] != '\0'; i++)
    {
    }
    if (i < 4)
        input_numbers[i] = '0' + num;
    refresh_number();
}
void delet_number()
{
    int i;
    for (i = 0; i < 4 && input_numbers[i] != '\0'; i++)
    {
    }
    if (i - 1 < 4)
        input_numbers[i - 1] = '\0';
    refresh_number();
}

static void help(void)
{
    ts_print_ascii_logo(16);
    print_version();
    printf("\n");
    printf("Usage: ts_test [-r <rotate_value>] [--version] [--help]\n");
    printf("\n");
    printf("-r --rotate\n");
    printf("        <rotate_value> 0 ... no rotation; 0 degree (default)\n");
    printf("                       1 ... clockwise orientation; 90 degrees\n");
    printf("                       2 ... upside down orientation; 180 degrees\n");
    printf("                       3 ... counterclockwise orientation; 270 degrees\n");
    printf("-h --help\n");
    printf("                       print this help test\n");
    printf("-v --version\n");
    printf("                       print version information only\n");
    printf("\n");
    printf("Example (Linux): ts_test -r $(cat /sys/class/graphics/fbcon/rotate)\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    struct tsdev *ts;
    int x, y;
    unsigned int i;
    unsigned int mode = 0;
    int quit_pressed = 0;

    signal(SIGSEGV, sig); //这三个是程序中断退出信号
    signal(SIGINT, sig);  //按Ctrl+c退出程序
    signal(SIGTERM, sig); //kill(1)终止

    while (1)
    {
        const struct option long_options[] = {
            {"help", no_argument, NULL, 'h'},
            {"rotate", required_argument, NULL, 'r'},
            {"version", no_argument, NULL, 'v'},
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "hvr:", long_options, &option_index);

        errno = 0;
        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            help();
            return 0;

        case 'v':
            print_version();
            return 0;

        case 'r':
            /* extern in fbutils.h */
            rotation = atoi(optarg);
            if (rotation < 0 || rotation > 3)
            {
                help();
                return 0;
            }
            break;

        default:
            help();
            return 0;
        }

        if (errno)
        {
            char str[9];

            sprintf(str, "option ?");
            str[7] = c & 0xff;
            perror(str);
        }
    }

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

    x = xres / 2;
    y = yres / 2;

    for (i = 0; i < NR_COLORS; i++)
        setcolor(i, palette[i]);

    /* Initialize buttons */
    int btn_number_w = xres / 7;
    int btn_number_h = yres / 5;

    memset(&buttons, 0, sizeof(buttons));
    for (i = 0; i < NR_BUTTONS; i++)
    {
        buttons[i].w = btn_number_w;
        buttons[i].h = btn_number_h;
        buttons[i].btn_colidx[0] = 2;
        buttons[i].btn_colidx[1] = 3;
        buttons[i].border_colidx[0] = buttons[i].border_colidx[1] = 0;
        buttons[i].font_colidx[0] = buttons[i].font_colidx[1] = 1;
    }
    buttons[10].btn_colidx[0] = buttons[11].btn_colidx[0] = 4;
    buttons[11].btn_colidx[1] = buttons[11].btn_colidx[1] = 5;

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
    buttons[10].text = "#";
    buttons[11].text = "*";

    refresh_screen();

    while (1)
    {
        struct ts_sample samp;
        int ret;

        /* Show the cross */
        if ((mode & 15) != 1)
            put_cross(x, y, 4 | XORMODE);

        ret = ts_read(ts, &samp, 1);
        samp.x = xres - samp.x;

        /* Hide it */
        if ((mode & 15) != 1)
            put_cross(x, y, 4 | XORMODE);

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
            if (button_handle(&buttons[i], samp.x, samp.y, samp.pressure))
                switch (i)
                {
                case 10: //#
                    break;
                case 11: //删除
                    delet_number();
                    break;
                default:
                    input_number(i);
                    break;
                }

        //printf("%ld.%06ld: %6d %6d %6d\n", samp.tv.tv_sec, samp.tv.tv_usec, samp.x, samp.y, samp.pressure);

        if (samp.pressure > 0)
        {
            if (mode == 0x80000001)
                line(x, y, samp.x, samp.y, 2);
            x = samp.x;
            y = samp.y;
            mode |= 0x80000000;
        }
        else
            mode &= ~0x80000000;
        if (quit_pressed)
            break;
    }
    fillrect(0, 0, xres - 1, yres - 1, 0);
    close_framebuffer();
    ts_close(ts);

    return 0;
}
