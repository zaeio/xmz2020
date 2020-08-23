#define BUFF_H1 0x55
#define BUFF_H2 0x55
#define BUFF_H3 0x55

#define BUFF_CMD_IMG 0x01    //图像
#define BUFF_CMD_AUDIO 0x02  //音频
#define BUFF_CMD_UNLOCK 0x03   //开锁
#define BUFF_CMD_CALL 0x04   //拨号请求
#define BUFF_CMD_ANSWER 0x05 //接听应答
#define BUFF_CMD_CAMERA 0x06 //打开监控
#define BUFF_CMD_HANGUP 0x07 //挂断

void setup_client_tcp();
int setup_server_tcp();

void send_Vframe(char *graymap);//
void send_unlock();
void send_answer();
void send_call();
void send_camera(char flag);
void send_hangup(char flag);
void send_pcm();