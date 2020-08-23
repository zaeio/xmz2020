struct font_struct
{
        int width, height, bytes_per_line;
        int offset_num, offset_capital, offset_lower;
        int index_hash, index_star;
        int index_answer, index_hangup;
        int index_microphone, index_camera;
        int index_lock;
};

unsigned char fontdata_24x24[12][72];
unsigned char fontdata_48x48[1][288];
unsigned char fontdata_64x64[17][512];

extern struct font_struct font_vga_24x24, font_vga_64x64;

unsigned char CS_48x48_sysname[8][288];       //智能楼宇对讲系统
unsigned char CS_48x48_input_number[6][288];  //请输入门牌号
unsigned char CS_48x48_press_to_call[6][288]; //按#号键呼叫
unsigned char CS_48x48_calling[4][288];       //正在呼叫
unsigned char CS_48x48_cancel[6][288];        //按#号键取消
unsigned char CS_48x48_wrong_number[5][288];  //门牌号错误
unsigned char CS_48x48_retry[3][288];         //请重试
unsigned char CS_48x48_online[3][288];        //已接通
