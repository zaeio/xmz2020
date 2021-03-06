/*
 * fbutils-linux.c
 *
 * Utility routines for framebuffer interaction
 *
 * Copyright 2002 Russell King and Doug Lowder
 *
 * This file is placed under the GPL.  Please see the
 * file COPYING for details.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>

#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>

#include "font.h"
#include "font_big.h"
#include "fbutils.h"

union multiptr {
        uint8_t *p8;
        uint16_t *p16;
        uint32_t *p32;
};

static int32_t con_fd, last_vt = -1;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;
static unsigned char *fbuffer;
static unsigned char **line_addr;
static int32_t fb_fd;
static int32_t bytes_per_pixel;
static uint32_t transp_mask;
static uint32_t colormap[256];
uint32_t xres, yres;
uint32_t xres_orig, yres_orig;
int8_t rotation;
int8_t alternative_cross;

static char *defaultfbdevice = "/dev/fb0";
static char *defaultconsoledevice = "/dev/tty";
static char *fbdevice;
static char *consoledevice;

#define VTNAME_LEN 128

int open_framebuffer(void)
{
        struct vt_stat vts;
        char vtname[VTNAME_LEN];
        int32_t fd, nr;
        uint32_t y, addr;

        if ((fbdevice = getenv("TSLIB_FBDEVICE")) == NULL)
                fbdevice = defaultfbdevice;

        if ((consoledevice = getenv("TSLIB_CONSOLEDEVICE")) == NULL)
                consoledevice = defaultconsoledevice;

        if (strcmp(consoledevice, "none") != 0)
        {
                if (strlen(consoledevice) >= VTNAME_LEN)
                        return -1;

                sprintf(vtname, "%s%d", consoledevice, 1);
                fd = open(vtname, O_WRONLY);
                if (fd < 0)
                {
                        perror("open consoledevice");
                        return -1;
                }

                if (ioctl(fd, VT_OPENQRY, &nr) < 0)
                {
                        close(fd);
                        perror("ioctl VT_OPENQRY");
                        return -1;
                }
                close(fd);

                sprintf(vtname, "%s%d", consoledevice, nr);

                con_fd = open(vtname, O_RDWR | O_NDELAY);
                if (con_fd < 0)
                {
                        perror("open tty");
                        return -1;
                }

                if (ioctl(con_fd, VT_GETSTATE, &vts) == 0)
                        last_vt = vts.v_active;

                if (ioctl(con_fd, VT_ACTIVATE, nr) < 0)
                {
                        perror("VT_ACTIVATE");
                        close(con_fd);
                        return -1;
                }

#ifndef TSLIB_NO_VT_WAITACTIVE
                if (ioctl(con_fd, VT_WAITACTIVE, nr) < 0)
                {
                        perror("VT_WAITACTIVE");
                        close(con_fd);
                        return -1;
                }
#endif

                if (ioctl(con_fd, KDSETMODE, KD_GRAPHICS) < 0)
                {
                        perror("KDSETMODE");
                        close(con_fd);
                        return -1;
                }
        }

        fb_fd = open(fbdevice, O_RDWR);
        if (fb_fd == -1)
        {
                perror("open fbdevice");
                return -1;
        }

        if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0)
        {
                perror("ioctl FBIOGET_FSCREENINFO");
                close(fb_fd);
                return -1;
        }

        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0)
        {
                perror("ioctl FBIOGET_VSCREENINFO");
                close(fb_fd);
                return -1;
        }

        xres_orig = var.xres;
        yres_orig = var.yres;

        if (rotation & 1)
        {
                /* 1 or 3 */
                y = var.yres;
                yres = var.xres;
                xres = y;
        }
        else
        {
                /* 0 or 2 */
                xres = var.xres;
                yres = var.yres;
        }

        fbuffer = mmap(NULL,
                       fix.smem_len,
                       PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
                       fb_fd,
                       0);

        if (fbuffer == (unsigned char *)-1)
        {
                perror("mmap framebuffer");
                close(fb_fd);
                return -1;
        }
        memset(fbuffer, 0, fix.smem_len);

        bytes_per_pixel = (var.bits_per_pixel + 7) / 8;
        transp_mask = ((1 << var.transp.length) - 1) << var.transp.offset; /* transp.length unlikely > 32 */
        line_addr = malloc(sizeof(*line_addr) * var.yres_virtual);
        addr = 0;
        for (y = 0; y < var.yres_virtual; y++, addr += fix.line_length)
                line_addr[y] = fbuffer + addr;

        return 0;
}

void close_framebuffer(void)
{
        memset(fbuffer, 0, fix.smem_len);
        munmap(fbuffer, fix.smem_len);
        close(fb_fd);

        if (strcmp(consoledevice, "none") != 0)
        {
                if (ioctl(con_fd, KDSETMODE, KD_TEXT) < 0)
                        perror("KDSETMODE");

                if (last_vt >= 0)
                        if (ioctl(con_fd, VT_ACTIVATE, last_vt))
                                perror("VT_ACTIVATE");

                close(con_fd);
        }

        free(line_addr);

        xres = 0;
        yres = 0;
        rotation = 0;
}

void put_cross(int32_t x, int32_t y, uint32_t colidx)
{
        line(x - 10, y, x - 2, y, colidx);
        line(x + 2, y, x + 10, y, colidx);
        line(x, y - 10, x, y - 2, colidx);
        line(x, y + 2, x, y + 10, colidx);

        if (!alternative_cross)
        {
                line(x - 6, y - 9, x - 9, y - 9, colidx + 1);
                line(x - 9, y - 8, x - 9, y - 6, colidx + 1);
                line(x - 9, y + 6, x - 9, y + 9, colidx + 1);
                line(x - 8, y + 9, x - 6, y + 9, colidx + 1);
                line(x + 6, y + 9, x + 9, y + 9, colidx + 1);
                line(x + 9, y + 8, x + 9, y + 6, colidx + 1);
                line(x + 9, y - 6, x + 9, y - 9, colidx + 1);
                line(x + 8, y - 9, x + 6, y - 9, colidx + 1);
        }
        else if (alternative_cross == 1)
        {
                line(x - 7, y - 7, x - 4, y - 4, colidx + 1);
                line(x - 7, y + 7, x - 4, y + 4, colidx + 1);
                line(x + 4, y - 4, x + 7, y - 7, colidx + 1);
                line(x + 4, y + 4, x + 7, y + 7, colidx + 1);
        }
}

//绘制大字符
void put_char_big(int x, int y, char c_ascii, int fontsize, int colidx)
{
        int font_index;
        struct font_struct *font_info;
        unsigned char *font_data;

        switch (fontsize)
        {
        case 24:
                font_info = &font_vga_24x24;
                font_data = &fontdata_24x24;
                break;
        case 64:
                font_info = &font_vga_64x64;
                font_data = &fontdata_64x64;
                break;
        default:
                font_info = &font_vga_64x64;
                font_data = &fontdata_64x64;
                break;
        }

        font_index = c_ascii - '0';           //先判断是数字，大写字母还是小写字母
        if (c_ascii >= '0' && c_ascii <= '9') //数字
        {
                font_index = c_ascii - '0' + font_info->offset_num;
        }
        else if (c_ascii >= 'A' && c_ascii <= 'Z') //大写
        {
                font_index = c_ascii - 'A' + font_info->offset_capital;
        }
        else if (c_ascii >= 'a' && c_ascii <= 'z') //小写
        {
                font_index = c_ascii - 'a' + font_info->offset_lower;
        }
        else
        {
                switch (c_ascii)
                {
                case '#':
                        font_index = font_info->index_hash;
                        break;
                case '*':
                        font_index = font_info->index_star;
                        break;
                case '[':
                        font_index = font_info->index_answer;
                        break;
                case ']':
                        font_index = font_info->index_hangup;
                        break;
                case '&':
                        font_index = font_info->index_microphone;
                        break;
                case '$':
                        font_index = font_info->index_lock;
                        break;
                case '@':
                        font_index = font_info->index_camera;
                        break;
                default:
                        break;
                }
        }

        //printf("width = %d   height = %d   bytes_per_line = %d\n", font_info->width, font_info->height, font_info->bytes_per_line);

        //打印
        int row, i, bit;
        unsigned char bits;
        for (row = 0; row < font_info->height; row++) //行
        {
                for (i = 0; i < font_info->bytes_per_line; i++) //每行的字节
                {
                        bits = font_data[font_index * font_info->bytes_per_line * fontsize + row * font_info->bytes_per_line + i]; //读取一个字节，二维数组转一维读取
                        for (bit = 0; bit < 8; bit++, bits <<= 1)                                                                  //每个字节8bit
                        {
                                if (bits & 0x80)
                                {
                                        pixel(x + bit + i * 8, y + row, colidx);
                                }
                        }
                }
        }
}

static void put_char(int32_t x, int32_t y, int32_t c, int32_t colidx)
{
        int32_t i, j, bits;

        for (i = 0; i < font_vga_8x8.height; i++)
        {
                bits = font_vga_8x8.data[font_vga_8x8.height * c + i];
                for (j = 0; j < font_vga_8x8.width; j++, bits <<= 1)
                        if (bits & 0x80)
                                pixel(x + j, y + i, colidx);
        }
}

void put_const_string(int x, int y, char *s, int strl, int fontsize, uint32_t colidx)
{
        int i, bytei, row, bit;
        int bpl = fontsize / 8; //bytes per line
        unsigned char bits;

        for (i = 0; i < strl; i++) //strl个字符
        {
                for (row = 0; row < fontsize; row++) //行
                {
                        for (bytei = 0; bytei < bpl; bytei++) //每行的字节
                        {
                                bits = s[(i * bpl * fontsize) + row * bpl + bytei]; //读取一个字节，这里二维数组转一维读取
                                for (bit = 0; bit < 8; bit++, bits <<= 1)           //每个字节8bit
                                {
                                        if (bits & 0x80)
                                        {
                                                pixel(x + bit + bytei * 8 + i * fontsize, y + row, colidx);
                                        }
                                }
                        }
                }
        }
}

//中心点坐标、字符串字模、字符串长度、字体大小、颜色索引
void put_const_string_center(int x_c, int y_c, char *s, int strl, int fontsize, uint32_t colidx)
{
        int x, y;
        x = x_c - strl * fontsize / 2;
        y = y_c - fontsize / 2;

        put_const_string(x, y, s, strl, fontsize, colidx);
}

void put_string(int32_t x, int32_t y, char *s, int fontsize, uint32_t colidx)
{
        int32_t i;
        for (i = 0; *s; i++, x += fontsize, s++)
                put_char_big(x, y, *s, fontsize, colidx);
}

void put_string_center(int32_t x, int32_t y, char *s, int fontsize, uint32_t colidx)
{
        size_t sl = strlen(s);
        put_string(x - sl * fontsize / 2, y - fontsize / 2, s, fontsize, colidx);
}

void setcolor(uint32_t colidx, uint32_t value)
{
        uint32_t res;
        uint16_t red, green, blue;
        struct fb_cmap cmap;

        if (colidx > 255)
        {
#ifdef DEBUG
                fprintf(stderr, "WARNING: color index = %u, must be <256\n",
                        colidx);
#endif
                return;
        }
        switch (bytes_per_pixel)
        {
        default:
        case 1:
                res = colidx;
                red = (value >> 8) & 0xff00;
                green = value & 0xff00;
                blue = (value << 8) & 0xff00;
                cmap.start = colidx;
                cmap.len = 1;
                cmap.red = &red;
                cmap.green = &green;
                cmap.blue = &blue;
                cmap.transp = NULL;

                if (ioctl(fb_fd, FBIOPUTCMAP, &cmap) < 0)
                        perror("ioctl FBIOPUTCMAP");
                break;
        case 2:
        case 3:
        case 4:
                red = (value >> 16) & 0xff;
                green = (value >> 8) & 0xff;
                blue = value & 0xff;
                res = ((red >> (8 - var.red.length)) << var.red.offset) |
                      ((green >> (8 - var.green.length)) << var.green.offset) |
                      ((blue >> (8 - var.blue.length)) << var.blue.offset);
        }
        colormap[colidx] = res;
}

uint32_t *convert_rgb_format(char *rgbmap, int width, int height)
{
        uint16_t red, green, blue;
        uint32_t *res;
        int i, j;

        res = (uint32_t *)malloc(sizeof(uint32_t) * width * height);
        printf("var.red.length = %d,  var.green.length = %d,  var.blue.length = %d\n", var.red.length, var.green.length, var.blue.length);
        printf("var.red.offset = %d,  var.green.offset = %d,  var.blue.offset = %d\n", var.red.offset, var.green.offset, var.blue.offset);

        for (i = 0; i < height; i++)
                for (j = 0; j < width; j++)
                {
                        red = (rgbmap[i * width + j] >> 16) & 0xff;
                        green = (rgbmap[i * width + j] >> 8) & 0xff;
                        blue = rgbmap[i * width + j] & 0xff;
                        res = ((red >> (8 - var.red.length)) << var.red.offset) |
                              ((green >> (8 - var.green.length)) << var.green.offset) |
                              ((blue >> (8 - var.blue.length)) << var.blue.offset);
                }
        if (res == NULL)
                printf("res == NULL\n");
        return res;
}

uint32_t *yuv420_to_rgb(char *yuv_data, int width, int height)
{
        int size = width * height;
        int i, j;
        uint32_t *RGBmap;

        RGBmap = (uint32_t *)malloc(sizeof(uint32_t) * size);
        for (i = 0; i < height; i++)
        {
                for (j = 0; j < width; j++)
                {
                        char Y = yuv_data[i * width + j];
                        char U = yuv_data[(int)(size + (i / 2) * (width / 2) + j / 2)];
                        char V = yuv_data[(int)(size * 1.25 + (i / 2) * (width / 2) + j / 2)];

                        //printf("Y = %X,  U = %X,  V = %X\n", Y, U, V);

                        char R = (char)(1.0 * Y + 1.402 * (V - 128));
                        char G = (char)(1.0 * Y - 0.344 * (U - 128) - 0.714 * (V - 128));
                        char B = (char)(1.0 * Y + 1.772 * (U - 128));

                        if (R < 0)
                                R = 0;
                        if (G < 0)
                                G = 0;
                        if (B < 0)
                                B = 0;
                        if (R > 255)
                                R = 255;
                        if (G > 255)
                                G = 255;
                        if (B > 255)
                                B = 255;
                        //printf("R = %X,  G = %X,  B = %X\n", R, G, B);

                        RGBmap[i * width + j] = ((uint32_t)B << 16) | ((uint32_t)G << 8) | ((uint32_t)R);
                        //printf("0x%06X\n", RGBmap[i * width + j]);
                }
        }
        return RGBmap;
}

char *yuv420_to_gray(char *yuv_data, int width, int height)
{
        int size = width * height;
        int i, j;
        char *graymap;

        graymap = (char *)malloc(sizeof(char) * size);
        for (i = 0; i < height; i++)
        {
                for (j = 0; j < width; j++)
                {
                        char Y = yuv_data[i * width + j];

                        if (Y < 0)
                                Y = 0;
                        if (Y > 255)
                                Y = 255;

                        graymap[i * width + j] = Y;
                        //printf("0x%06X\n", RGBmap[i * width + j]);
                }
        }
        return graymap;
}

static void __pixel_loc(int32_t x, int32_t y, union multiptr *loc)
{
        switch (rotation)
        {
        case 0:
        default:
                loc->p8 = line_addr[y] + x * bytes_per_pixel;
                break;
        case 1:
                loc->p8 = line_addr[x] + (yres - y - 1) * bytes_per_pixel;
                break;
        case 2:
                loc->p8 = line_addr[yres - y - 1] + (xres - x - 1) * bytes_per_pixel;
                break;
        case 3:
                loc->p8 = line_addr[xres - x - 1] + y * bytes_per_pixel;
                break;
        }
}

static inline void __setpixel(union multiptr loc, uint32_t xormode, uint32_t color)
{
        switch (bytes_per_pixel)
        {
        case 1:
        default:
                if (xormode)
                        *loc.p8 ^= color;
                else
                        *loc.p8 = color;
                *loc.p8 |= transp_mask;
                break;
        case 2:
                if (xormode)
                        *loc.p16 ^= color;
                else
                        *loc.p16 = color;
                *loc.p16 |= transp_mask;
                break;
        case 3:
                if (xormode)
                {
                        *loc.p8++ ^= (color >> 16) & 0xff;
                        *loc.p8++ ^= (color >> 8) & 0xff;
                        *loc.p8 ^= color & 0xff;
                }
                else
                {
                        *loc.p8++ = (color >> 16) & 0xff;
                        *loc.p8++ = (color >> 8) & 0xff;
                        *loc.p8 = color & 0xff;
                }
                *loc.p8 |= transp_mask;
                break;
        case 4:
                if (xormode)
                        *loc.p32 ^= color;
                else
                        *loc.p32 = color;
                *loc.p32 |= transp_mask;
                break;
        }
}

void put_rgb_map(int x, int y, uint32_t *rgbmap, int width, int height)
{
        uint32_t xormode;
        union multiptr loc;
        int i, j, x_pixel, y_pixel;

        // printf("putting rgb\n");
        for (i = 0; i < height; i++)
        {
                for (j = 0; j < width; j++)
                {
                        x_pixel = x + j;
                        y_pixel = y + i;

                        if ((x_pixel < 0) || (x_pixel >= 1024) || (y_pixel < 0) || (y_pixel >= 600))
                        {
                                printf("pixel lacation out of range!\n");
                                return;
                        }

                        __pixel_loc(x_pixel, y_pixel, &loc);
                        __setpixel(loc, 0, rgbmap[i * width + j]);
                }
        }
}

void put_gray_map(int x, int y, char *graymap, int width, int height)
{
        uint32_t xormode;
        union multiptr loc;
        int i, j, x_pixel, y_pixel;

        // printf("putting rgb\n");
        for (i = 0; i < height; i++)
        {
                for (j = 0; j < width; j++)
                {
                        x_pixel = x + j;
                        y_pixel = y + i;

                        if ((x_pixel < 0) || (x_pixel >= 1024) || (y_pixel < 0) || (y_pixel >= 600))
                        {
                                printf("pixel lacation out of range!\n");
                                return;
                        }

                        char R = graymap[i * width + j];
                        char G = graymap[i * width + j];
                        char B = graymap[i * width + j];

                        if (R < 0)
                                R = 0;
                        if (G < 0)
                                G = 0;
                        if (B < 0)
                                B = 0;
                        if (R > 255)
                                R = 255;
                        if (G > 255)
                                G = 255;
                        if (B > 255)
                                B = 255;

                        uint32_t RGB = ((uint32_t)B << 16) | ((uint32_t)G << 8) | ((uint32_t)R);

                        __pixel_loc(x_pixel, y_pixel, &loc);
                        __setpixel(loc, 0, RGB);
                }
        }
}

void pixel(int32_t x, int32_t y, uint32_t colidx)
{
        uint32_t xormode;
        union multiptr loc;

        if ((x < 0) || ((uint32_t)x >= xres) ||
            (y < 0) || ((uint32_t)y >= yres))
                return;

        xormode = colidx & XORMODE;
        colidx &= ~XORMODE;

        if (colidx > 255)
        {
#ifdef DEBUG
                fprintf(stderr, "WARNING: color value = %u, must be <256\n",
                        colidx);
#endif
                return;
        }
        __pixel_loc(x, y, &loc);
        __setpixel(loc, xormode, colormap[colidx]);
}

void line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colidx)
{
        int32_t tmp;
        int32_t dx = x2 - x1;
        int32_t dy = y2 - y1;

        if (abs(dx) < abs(dy))
        {
                if (y1 > y2)
                {
                        tmp = x1;
                        x1 = x2;
                        x2 = tmp;
                        tmp = y1;
                        y1 = y2;
                        y2 = tmp;
                        dx = -dx;
                        dy = -dy;
                }
                x1 <<= 16;
                /* dy is apriori >0 */
                dx = (dx << 16) / dy;
                while (y1 <= y2)
                {
                        pixel(x1 >> 16, y1, colidx);
                        x1 += dx;
                        y1++;
                }
        }
        else
        {
                if (x1 > x2)
                {
                        tmp = x1;
                        x1 = x2;
                        x2 = tmp;
                        tmp = y1;
                        y1 = y2;
                        y2 = tmp;
                        dx = -dx;
                        dy = -dy;
                }
                y1 <<= 16;
                dy = dx ? (dy << 16) / dx : 0;
                while (x1 <= x2)
                {
                        pixel(x1, y1 >> 16, colidx);
                        y1 += dy;
                        x1++;
                }
        }
}

void rect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colidx)
{
        line(x1, y1, x2, y1, colidx);
        line(x2, y1 + 1, x2, y2 - 1, colidx);
        line(x2, y2, x1, y2, colidx);
        line(x1, y2 - 1, x1, y1 + 1, colidx);
}

void fillrect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t colidx)
{
        int32_t tmp;
        uint32_t xormode;
        union multiptr loc;

        /* Clipping and sanity checking */
        if (x1 > x2)
        {
                tmp = x1;
                x1 = x2;
                x2 = tmp;
        }
        if (y1 > y2)
        {
                tmp = y1;
                y1 = y2;
                y2 = tmp;
        }

        if (x1 < 0)
                x1 = 0;
        if ((uint32_t)x1 >= xres)
                x1 = xres - 1;

        if (x2 < 0)
                x2 = 0;
        if ((uint32_t)x2 >= xres)
                x2 = xres - 1;

        if (y1 < 0)
                y1 = 0;
        if ((uint32_t)y1 >= yres)
                y1 = yres - 1;

        if (y2 < 0)
                y2 = 0;
        if ((uint32_t)y2 >= yres)
                y2 = yres - 1;

        if ((x1 > x2) || (y1 > y2))
                return;

        xormode = colidx & XORMODE;
        colidx &= ~XORMODE;

        if (colidx > 255)
        {
#ifdef DEBUG
                fprintf(stderr, "WARNING: color value = %u, must be <256\n",
                        colidx);
#endif
                return;
        }

        colidx = colormap[colidx];

        for (; y1 <= y2; y1++)
        {
                for (tmp = x1; tmp <= x2; tmp++)
                {
                        __pixel_loc(tmp, y1, &loc);
                        __setpixel(loc, xormode, colidx);
                        loc.p8 += bytes_per_pixel;
                }
        }
}
