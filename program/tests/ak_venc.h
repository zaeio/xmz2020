#ifndef _AK_VENC_H_
#define _AK_VENC_H_

enum ak_venc_error_type 
{
    ERROR_VENC_INIT_LIB_ERROR = (MODULE_ID_VENC << 24) + 0,
	ERROR_VENC_OPEN_LIB_ERROR,
    ERROR_VENC_OPEN_TOO_MANY,
    ERROR_VENC_INVALID_PARAM,
    ERROR_VENC_USER_NULL,
    ERROR_VENC_PARAM_CANNOT_DYNAMIC_SET
};

/* h.264 / h.265 encode control define */
enum bitrate_ctrl_mode {
    BR_MODE_CONST_QP,
    BR_MODE_CBR,
    BR_MODE_VBR,
    BR_MODE_LOW_LATENCY
};

/* video encode output type define */
enum encode_output_type {
    H264_ENC_TYPE,
    MJPEG_ENC_TYPE,
    HEVC_ENC_TYPE
};

enum profile_mode {
    PROFILE_MAIN,
    PROFILE_HIGH,
    PROFILE_BASE,
    PROFILE_C_BASE,
    PROFILE_HEVC_MAIN,
    PROFILE_HEVC_MAIN_STILL,
    PROFILE_HEVC_MAIN_INTRA,
    PROFILE_JPEG
};

enum chroma_mode {
    CHROMA_MONO,
    CHROMA_4_2_0
};

enum jpeg_quant_table_level {
    JPEG_QLEVEL_DEFAULT,
    JPEG_QLEVEL_HIGHEST,
    JPEG_QLEVEL_HIGH,
    JPEG_QLEVEL_LOW
};

enum smart_mode {
    SMART_DISABLE,
    SMART_LTR,
    SMART_CHANGING_GOPLEN,
    SMART_SKIP_FRAME    
};


struct venc_roi_param {
    int  enable;    //1 enable, 0 disable
    long top;
    long bottom;
    long left;
    long right;
    long delta_qp;
};

struct venc_param {
    unsigned short                width;
    unsigned short                height;
    unsigned short                fps; 
    unsigned short                goplen; 
    unsigned short                target_kbps;    
    unsigned short                max_kbps;              
    enum profile_mode             profile;             
    enum bitrate_ctrl_mode        br_mode;     
    unsigned short                initqp;        //Dynamic bit rate parameter[20,25]
    unsigned short                minqp;        //Dynamic bit rate parameter[20,25]
    unsigned short                maxqp;        //Dynamic bit rate parameter[45,50]
    enum jpeg_quant_table_level   jpeg_qlevel;
    enum chroma_mode              chroma_mode;
    enum encode_output_type       enc_out_type;//encode output type, h264 or jpeg or h265
    unsigned int                  max_picture_size;
    unsigned short                enc_level;
    enum smart_mode               smart_mode;            //0:disable smart, 1:mode of LTR, 2:mode of changing GOP length
    unsigned short                smart_goplen;        //smart goplen
    unsigned short                smart_quality;         //smart quality
    unsigned short                smart_static_value; //smart static value
    
};



struct venc_stat {
    unsigned short fps; 
    unsigned short goplen; 
    unsigned short kbps;              
    unsigned short max_picture_size;     
};


/**
 * ak_venc_get_version - get venc version
 * return: version string
 */
const char* ak_venc_get_version(void);

/**
 * ak_venc_open - open encoder and set encode param
 * @param[IN]: encode param
 * @param[OUT]: handle id
 * return: 0 success , others error code.
 */
int ak_venc_open(const struct venc_param *param, int *handle_id);


/**
 * ak_venc_encode_frame - encode single frame
 * @handle_id[IN]: handle id return by ak_venc_open
 * @frame[IN]: frame which you want to encode
 * @frame_len[IN]: lenght of frame
 * @mdinfo[IN]: md info array
 * @stream[OUT]: encode output buffer address
 * return: 0 success , others error code.
 */
int ak_venc_encode_frame(int handle_id, const unsigned char *frame,
        unsigned int frame_len, void *mdinfo, struct video_stream *stream);

/**
 * ak_venc_release_stream - release stream resource
 * @handle_id[IN]: handle id return by ak_venc_open
 * @stream[IN]: stream return by ak_venc_encode_frame()
 * return: 0 success , others error code.
 * notes:
 */
int ak_venc_release_stream(int handle_id, struct video_stream *stream);


/**
 * ak_venc_close - close video encode
 * @handle_id[IN]: handle id return by ak_venc_open()
 * return: 0 success , others error code.
 */
int ak_venc_close(int handle_id);

/**
 * ak_venc_set_attr - set venc params
 * @handle_id[IN]: handle id return by ak_venc_open
 * @param[IN]: param to set
 * return: 0 success , others error code.
 * notes:
 */
int ak_venc_set_attr(int handle_id, const struct venc_param *param);

/**
 * ak_venc_get_attr - get venc params
 * @handle_id[IN]: handle id return by ak_venc_open
 * @param[OUT]: params
 * return: 0 success , others error code.
 * notes:
 */
int ak_venc_get_attr(int handle_id, struct venc_param *param);

/**
 * ak_venc_request_idr - request I frame
 * @handle_id[IN]: handle id return by ak_venc_open
 * return: 0 success , others error code.
 * notes:
 */
int ak_venc_request_idr(int handle_id);


/**
 * ak_venc_get_rate_stat - on stream-encode, get encode rate stat info
 * @handle_id[IN]: handle id return by ak_venc_open
 * @stat[OUT]: stream rate stat info
 * return: 0 success, -1 failed
 * notes:
 */
int ak_venc_get_stat(int handle_id, struct venc_stat *stat);

int venc_rtp();

#endif
