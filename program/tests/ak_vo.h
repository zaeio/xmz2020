#ifndef _AK_VO_H
#define _AK_VO_H

#define DEV_NUM     0

/* vo module errno list */
enum ak_vo_errno {                                                            //错误代码
    ERROR_VO_INVALID_DEV_ID            = ( MODULE_ID_VO << 24 ) + 0,          //0 无效句柄ID
    ERROR_VO_OPENED_ALREADY            = ( MODULE_ID_VO << 24 ) + 1,          //1 vo已经打开
    ERROR_VO_OPENED_FB_DEV_FAILED      = ( MODULE_ID_VO << 24 ) + 2,          //2 fb设备打开失败
    ERROR_VO_FBGET_FSCREENINFO_FAILED  = ( MODULE_ID_VO << 24 ) + 3,          //3 fb获取固定参数失败
    ERROR_VO_FBGET_VSCREENINFO_FAILED  = ( MODULE_ID_VO << 24 ) + 4,          //4 fb获取可设参数失败
    ERROR_VO_FBPUT_VSCREENINFO_FAILED  = ( MODULE_ID_VO << 24 ) + 5,          //5 fb设置参数失败
    ERROR_VO_MEMORY_INIT_FAILED        = ( MODULE_ID_VO << 24 ) + 6,          //6 初始化内存失败
    ERROR_VO_RESOLUTION_INVALID        = ( MODULE_ID_VO << 24 ) + 7,          //7 vo的分辨率无效
    ERROR_VO_WRONG_OBJ_FORMAT          = ( MODULE_ID_VO << 24 ) + 8,          //8 图像格式错误
    ERROR_VO_ADD_OBJECT_FAILED         = ( MODULE_ID_VO << 24 ) + 9,          //9 图像添加失败
    ERROR_VO_WRONG_OBJECT_PARAM        = ( MODULE_ID_VO << 24 ) + 10,         //10 图像添加参数错误
};

enum ak_vo_layer {
    AK_VO_LAYER_BG_1 = 0X00,
    AK_VO_LAYER_BG_2,
    AK_VO_LAYER_BG_3,
    AK_VO_LAYER_BG_4,
    AK_VO_LAYER_BG_5,
    AK_VO_LAYER_BG_6,
    AK_VO_LAYER_BG_7,
    AK_VO_LAYER_BG_8,
    AK_VO_LAYER_VIDEO_1,
    AK_VO_LAYER_VIDEO_2,
    AK_VO_LAYER_VIDEO_3,
    AK_VO_LAYER_VIDEO_4,
    AK_VO_LAYER_VIDEO_5,
    AK_VO_LAYER_VIDEO_6,
    AK_VO_LAYER_VIDEO_7,
    AK_VO_LAYER_VIDEO_8,
    AK_VO_LAYER_GUI_1,
    AK_VO_LAYER_GUI_2,
    AK_VO_LAYER_GUI_3,
    AK_VO_LAYER_GUI_4,
    AK_VO_LAYER_GUI_5,
    AK_VO_LAYER_GUI_6,
    AK_VO_LAYER_GUI_7,
    AK_VO_LAYER_GUI_8,
    AK_VO_LAYER_NUM
};

struct ak_vo_param{
    int width;                  /* width of visable screen  */
    int height;                 /* height of visable screen */
    AK_GP_FORMAT    format;     /* format of pixel */
    AK_GP_ROTATE    rotate;     /* rotation mode */
};

struct ak_layer_pos{
    int top;            /* top position of the layer */
    int left;           /* left position of the layer */
    int width;          /* width of the  layer */
    int height;         /* height of the layer */
};

struct ak_layer_obj{
    int width;                      /* width of layer */
    int height;                     /* height of layer */
    struct ak_layer_pos	clip_pos;   /* postion info the clip picture */
    unsigned long dma_addr;         /* phyaddr of dma memory */
};

struct ak_tileyuv_obj{

    void *data;         /* pointer to the data */
};

struct ak_vo_obj{
    AK_GP_FORMAT    format;                 /* src object format */
    AK_GP_OPT cmd;                          /* command for the obj*/
    union{
        struct ak_layer_obj	layer;          /* src info of the layer */
        struct ak_tileyuv_obj	tileyuv;    /* src info of the fbd */
    }src_obj;
    struct ak_layer_pos	dst_layer;          /* dst position  info of the layer */

    int alpha;                              /* alpha value */
    struct ak_gp_colorkey   colorkey;       /* colorkey struct */
    unsigned int color_rect;                /* color Rectangular filled*/
};
#define vo_layer        src_obj.layer
#define vo_tileyuv      src_obj.tileyuv


/* layer input infor */
struct ak_vo_layer_in
{
    struct ak_layer_pos create_layer;   /* layer size and pos in screen */

    AK_GP_FORMAT    format;             /* layer format , only support the rgb except the yuv format */
    AK_GP_OPT   layer_opt;              /* command for the layer support the alpha or colorkey */

    int alpha;                          /* alpha value */
    struct ak_gp_colorkey   colorkey;   /* colorkey struct */
};

/* layer output inform */
struct ak_vo_layer_out
{
    unsigned long dma_addr; /* phyaddr of dma memory */
    int layer_size;         /* layer size */
    void *vir_addr;         /* layer virtual address */
};

/*
 * ak_vo_get_version	--	get vo version
 * return string of version
 */
const char* ak_vo_get_version(void);

/*
 * ak_vo_open   --  open vo device
 *@param[IN]    :   param of vo
 *@dev_no[IN]   :   dev num of device
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_open(struct ak_vo_param *param, int dev_no);

/*
 * ak_vo_get_resolution     --      get the resolution of vo
 * @width[OUT]      :       width of visable screen
 * @height[OUT]     :       height of visable screen
 * @fb_width[OUT]   :       width of framebuffer
 * @fb_height[OUT]  :       height of framebuffer
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_get_resolution(int *width, int* height, int *fb_width, int *fb_height);

/*
 * ak_vo_close  --  close vo device
 *@dev_no[IN]   :   dev num of device
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_close(int dev_no);

/*
 * ak_vo_create_bg_layer    --  ·  create the bg layer  
 * @bg_layer[IN]            :       pointer of struct ak_layer_pos, contains the size of layer and the pos
 *                                  in fb buffer;
 * @type[IN]                :       type of the layer;
 * @bg_info[OUT]            :       output the bg layer info
 * return AK_SUCCESS if successful, other value if failed
 */
int ak_vo_create_bg_layer(struct ak_vo_layer_in *bg_layer, enum ak_vo_layer type, struct ak_vo_layer_out *bg_info);

/*
 * ak_vo_create_video_layer     --  create the video layer  
 * @video_layer[IN]         :       pointer of struct ak_layer_pos, contains the size of layer and the pos
 *                                  in fb buffer;
 * @type[IN]                :       type of the layer;
 * return AK_SUCCESS if successful, other value if failed
 */
int ak_vo_create_video_layer(struct ak_vo_layer_in *video_layer, enum ak_vo_layer type);

/*
 * ak_vo_create_gui_layer   --      create the gui layer  
 * @gui_layer[IN]           :       pointer of struct ak_layer_pos, contains the size of layer and the pos
 *                                  in fb buffer;
 * @type[IN]                :       type of the layer;
 * @gui_info[OUT]           :       output the gui layer info
 * return AK_SUCCESS if successful, other value if failed
 */
int ak_vo_create_gui_layer(struct ak_vo_layer_in *gui_layer, enum ak_vo_layer type, struct ak_vo_layer_out *gui_info);

/*
 * ak_vo_set_layer_pos      --  set the layer position
 * @type[IN]  :                 the dst layer users want to change pos
 * @left[IN]  :                 left value;
 * @top[IN]   :                 top value
 * return AK_SUCCESS if successful, other value if failed
 */
int ak_vo_set_layer_pos(enum ak_vo_layer type, int left, int top);

/*
 * ak_vo_destroy_layer      --  destroy  layer  
 * @type[IN]                :       type of the layer;
 * return AK_SUCCESS if successful, other value if failed
 */
int ak_vo_destroy_layer(enum ak_vo_layer type);

/*
 * ak_vo_add_obj    --  add object to vo
 * @obj[IN]     :       pointer of struct ak_vo_obj
 * @layer_no[IN]:       dst layer to add obj
 * return AK_SUCCESS if successful, Error code if failed
 * */
int ak_vo_add_obj(struct ak_vo_obj	*obj, enum ak_vo_layer layer_no);

/* THIS cmd is provided to ak_vo_refresh_screen function
 * NOTE : you can combine several value of them with | operation
          A type group contains 8 layers listed above. 
          So a bit of the group value stands for a layer in sequence 
 **/
enum ak_vo_refresh_cmd
{
    AK_VO_REFRESH_BG_GROUP    = 0xff,
    AK_VO_REFRESH_VIDEO_GROUP = 0xff00,
    AK_VO_REFRESH_GUI_GROUP   = 0xff0000
};
/*
 * ak_vo_refresh_screen		--	refresh the screen
 * cmd[IN] : cmd to refresh the layer created before.
 *           the value can be seen above in enum ak_vo_refresh_cmd
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_refresh_screen(int cmd);

/*
 * ak_vo_set_layer_colorkey     --  set the  dst layer colorkey opt
 * @type[IN]  :                dst layer to set colorkey
 * @colorkey[IN]  :            colorkey value to be set
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_set_layer_colorkey(enum ak_vo_layer type, struct ak_gp_colorkey colorkey);

/*
 * ak_vo_set_layer_alpha        --  set the  dst layer transparent opt
 * @type[IN]  :                dst layer to set colorkey
 * @alpha[IN]  :            alpha value to be set
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_set_layer_alpha(enum ak_vo_layer type, int alpha);

/*
 * THIS buff num is used for seting the vo module using the buff to show screen
 */
enum ak_vo_buff_num
{
    AK_VO_BUFF_SINGLE = 0x00,
    AK_VO_BUFF_DOUBLE
};

/*
 * ak_vo_set_fbuffer_mode     --  set the  buffer mode
 * @mode[IN]  :                mode of the buff usage,value is from enum ak_vo_buff_num
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_set_fbuffer_mode(int mode);

/*
 * ak_vo_get_fbuffer_status  --  get the  current buffer use status
 * @cur_mode[OUT]        :            cur using on the module
 * @drv_sup[OUT]         :            cur mode that driver support 
 * return AK_SUCCESS if successful, Error code if failed
 */
int ak_vo_get_fbuffer_status(int *cur_mode, int *drv_sup);


#endif
