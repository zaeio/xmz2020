#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "fbutils.h"
#include "ak_common.h"
#include "ak_common_graphics.h"
#include "ak_vi.h"
#include "ak_ai.h"
#include "ak_ao.h"
#include "av_socket.h"

#define IMG_WIDTH 896
#define IMG_HEIGHT 600

//数据包，[包头，指令，数据]
//图像的数据长度超过了255
#define BUFF_CMD_IMG 0x01    //图像
#define BUFF_CMD_AUDIO 0x02  //音频
#define BUFF_CMD_LOCK 0x03   //开锁
#define BUFF_CMD_CALL 0x04   //拨号请求
#define BUFF_CMD_ANSWER 0x05 //接听应答

extern int send_sound_flag;
extern int recv_sound_flag;
int client_fd, server_fd, connfd;
struct sockaddr_in servaddr, connaddr; //服务器ip，接入的客户端ip
char *server_addr_str = "192.168.1.120";

pthread_t indoor_bell_thread, vi_thread, ai_thread;
pthread_t outdoor_vi_thread, outdoor_ai_thread;
pthread_t tcp_indoor_thread,tcp_outdoor_thread;
extern int play_bell_flag; //播放铃声使能
int outdoor_current_state = 0;
int indoor_current_state = 0;
extern int vi_capture_enable; //摄像持续使能

//建立语音的TCP连接
void setup_client_tcp()
{
        // １，创建TCP套接字
        client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1)
        {
                perror("create sound tcp failed");
                return;
        }

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(server_addr_str);
        servaddr.sin_port = htons(53000);
        // ２，向服务器发起连接请求
        if (connect(client_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        {
                perror("connect falied\n");
                // printf("tcp connect falied\n");
                // close(socket_fd);
        }
}

void setup_server_tcp()
{
        // １，创建TCP套接字
        server_fd = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0);
        socklen_t len = sizeof(servaddr);

        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(server_addr_str); // 绑定本机IP
        servaddr.sin_port = htons(53000);                      // 端口号，范围: 1 - 65535，最好用50000以上的端口防冲突

        // ２，绑定地址（IP + PORT）
        if (bind(server_fd, (struct sockaddr *)&servaddr, len) == -1)
        {
                perror("bind sound tcp fail");
                close(server_fd);
                return;
        }
        // ３，将套接字设置为监听状态
        // 设定之后的fd，就是监听套接字，专用来等待连接的
        if (listen(server_fd, 1 /*设定最大同时连接个数*/) == -1)
        {
                perror("listen fail");
                close(server_fd);
                return;
        }
        // ４，坐等对方的连接请求... ...
        // 返回的connfd，就是已连接套接字，专用来读写通信的
        len = sizeof(connaddr);
        bzero(&connaddr, len);

        connfd = accept(server_fd, (struct sockaddr *)&connaddr /*获取对方的地址信息*/, &len);
        //接收对方发来的语音消息，
        if (connfd > 0)
        {
                printf("new connection: [%s:%hu]\n", inet_ntoa(connaddr.sin_addr), ntohs(connaddr.sin_port));
                pthread_create(&tcp_indoor_thread, NULL, (void *)receive_indoor_routine, NULL);
        }
        else
        {
                perror("accept() failed");
                close(connfd);
                close(server_fd);
                return;
        }
}

//寻找包头，返回指令
char get_header(char *buff, int length)
{
        int i;
        if (length >= 4)
                for (i = 0; i <= length - 4; i++)
                {
                        // printf("buff[%d] = %d  buff[%d] = %d   buff[%d] = %d \n", i, buff[i], i + 1, buff[i + 1], i + 2, buff[i + 2]);
                        // printf("buff[%d] = %d\n", i + 3, buff[i + 3]);
                        if (buff[i] == 0x55 && buff[i + 1] == 0x55 && buff[i + 2] == 0x55)
                        {
                                if (buff[i + 3] >= 0x01 && buff[i + 3] <= 0x05)
                                {
                                        // printf("headder 123 confirmed  buff[%d] = %d\n", i + 3, buff[i + 3]);
                                        return buff[i + 3];
                                }
                        }
                }
        return 0;
}

void send_Vframe(char *graymap) //
{
        int i;
        char buff[4] = {0};

        buff[0] = 0x55; //包头
        buff[1] = 0x55;
        buff[2] = 0x55;
        buff[3] = BUFF_CMD_IMG;
        send(client_fd, buff, 4, 0);
        printf("BUFF_CMD_IMG send\n");
        // send(socket_fd, graymap,IMG_WIDTH*IMG_HEIGHT,0);
        for (i = 0; i < IMG_HEIGHT; i++)
        {
                send(client_fd, &graymap[i * IMG_WIDTH], IMG_WIDTH, 0);
        }
}

void send_unlock()
{
        char *buff[4] = {0};

        buff[0] = 0x55; //包头
        buff[1] = 0x55;
        buff[2] = 0x55;
        buff[3] = BUFF_CMD_LOCK;
        send(client_fd, buff, 4, 0);
        printf("BUFF_CMD_LOCK send\n");
}

void send_answer()
{
        char buff[4] = {0};

        buff[0] = 0x55; //包头
        buff[1] = 0x55;
        buff[2] = 0x55;
        buff[3] = BUFF_CMD_ANSWER;
        send(connfd, buff, 4, 0);

        printf("BUFF_CMD_ANSWER send\n");
}

void send_call()
{
        char buff[4] = {0};

        buff[0] = 0x55; //包头
        buff[1] = 0x55;
        buff[2] = 0x55;
        buff[3] = BUFF_CMD_CALL;
        send(client_fd, buff, 4, 0);

        printf("BUFF_CMD_CALL send\n");
}

void send_pcm()
{
        char *pcmfilename = "/mnt/frame/ak_ao_test.pcm";
        FILE *fp_pcm = NULL;
        struct stat filestat;
        char *pcmbuf;

        fp_pcm = fopen(pcmfilename, "r");
        if (NULL == fp_pcm)
        {
                printf("open file error\n");
                return;
        }

        stat(pcmfilename, &filestat); //获取文件状态，主要是大小
        pcmbuf = (char *)calloc(filestat.st_size, sizeof(char));
        fread(pcmbuf, sizeof(char), filestat.st_size, fp_pcm);

        send(client_fd, pcmbuf, filestat.st_size, 0);
        printf("send size = %d\n", filestat.st_size);
        close(client_fd);
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
                        printf("BUFF_CMD_IMG\n");
                        //先结束上一帧
                        if (framecount > 0)
                        {
                                put_gray_map(0, 0, graymap, IMG_WIDTH, IMG_HEIGHT);
                                free(graymap);
                                recvcount = 0;
                        }
                        framecount++;
                        graymap = (char *)calloc(IMG_WIDTH * IMG_HEIGHT, sizeof(char));
                }
                break;
                case BUFF_CMD_CALL:
                {
                        printf("BUFF_CMD_CALL\n");
                        indoor_current_state = 1;
                        play_bell_flag = 1;
                        pthread_create(&indoor_bell_thread, NULL, (void *)play_bell_routine, NULL);
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
                                        if (recvcount + i < IMG_WIDTH * IMG_HEIGHT)
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

void receive_outdoor_routine()
{
        char buf[896] = {0};

        printf("receiving\n");
        int recvlen = 0;
        while ((recvlen = recv(client_fd, buf, 896, 0)) > 0)
        {
                char file_name[255];
                printf("recvlen = %d\n", recvlen);
                switch (get_header(buf, recvlen))
                {
                break;
                case BUFF_CMD_ANSWER:
                {
                        printf("BUFF_CMD_ANSWER\n");
                        outdoor_current_state = 2; //STATE_ONLINE
                        vi_capture_enable = 1;
                        pthread_create(&vi_thread, NULL, (void *)vi_capture_loop, NULL);
                }
                break;
                default:
                break;
                }
        }
        printf("receive finished\n");
        close(server_fd);
        close(connfd);

        //退出线程
        pthread_exit(NULL);
}
