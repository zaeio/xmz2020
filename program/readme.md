# 程序说明
## GUI
`init_widget()`用于控件初始化，控件目前包含`button`和`textbox`。`textbox`目前不支持光标。  

下面是大字体，定义在`font_big.c`和`font_big.h`中。目前未包含大小写字母，因此button和textbox文本、字符串显示中还不能包含字母。 
```
struct font_struct
{
    int width, height, bytes_per_line;
    int offset_num, offset_capital, offset_lower;
    int index_hash, index_star;
};
unsigned char fontdata_24x24[12][72];
unsigned char fontdata_64x64[12][512];
```
下面是固定文本字符串字模，用于不可变文本和中文。显示任意字符串使用`put_string()`或`put_string_center()`；显示固定字符串使用`put_const_string()`；显示大字符使用`put_char_big()`。  
```
unsigned char CS_48x48_sysname[8][288];//智能楼宇对讲系统
unsigned char CS_48x48_input_number[6][288];//请输入门牌号
unsigned char CS_48x48_press_to_call[6][288];//按#号键呼叫
unsigned char CS_48x48_calling[4][288];//正在呼叫
unsigned char CS_48x48_cancel[6][288];//按#号键取消
unsigned char CS_48x48_rowng_number[5][288];//门牌号错误
unsigned char CS_48x48_retry[3][288];//请重试
```

按钮事件在GUI.c中，如下
```
//按钮事件
for (i = 0; i < NR_BUTTONS; i++)
    if (button_handle(&buttons[i], samp.x, samp.y, samp.pressure))
        switch (i)
        {
        case 10: //#
        {
            if (current_state == STATE_WELCOME)
            {
                int floor, num;
                floor = (textboxes[0].text[0] - '0') * 10 + (textboxes[0].text[1] - '0');
                num = (textboxes[0].text[2] - '0') * 10 + (textboxes[0].text[3] - '0');
                if (floor > 0 && floor <= 10 && num > 0 && num <= 4)
                    current_state = STATE_CALL;
                else
                    current_state = STATE_NUM_ERROR;
            }
            else if (current_state == STATE_CALL)
            {
                current_state = STATE_WELCOME;
            }
            print_usage_info();
        }
        break;
        case 11: //删除
            textbox_delchar(&textboxes[0]);
            if (current_state == STATE_NUM_ERROR)
            {
                current_state = STATE_WELCOME;
                print_usage_info();
            }
            break;
        default:
            textbox_addchar(&textboxes[0], '0' + i);
            break;
        }
```
这一段定义了拨号界面的按钮事件，接收数字输入，并显示`正在呼叫`、`号码错误`等信息。

## 视频IO
执行`sensor.sh install ar0230`以后会出现/dev/video0