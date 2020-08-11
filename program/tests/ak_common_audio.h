#ifndef _AK_COMMON_AUDIO_H_
#define _AK_COMMON_AUDIO_H_

#define    EQ_MAX_BANDS    10

/* audio function status */
enum audio_func_status 
{
	AUDIO_FUNC_DISABLE = 0x00,
	AUDIO_FUNC_ENABLE,
};

/* audio channel type */
enum ak_audio_channel_type 
{
	AUDIO_CHANNEL_RESERVED = 0x00,
	AUDIO_CHANNEL_MONO,
	AUDIO_CHANNEL_STEREO,
};

enum ak_audio_type 
{
	AK_AUDIO_TYPE_UNKNOWN,
	AK_AUDIO_TYPE_MIDI,
	AK_AUDIO_TYPE_MP3,
	AK_AUDIO_TYPE_AMR,
	AK_AUDIO_TYPE_AAC,
	AK_AUDIO_TYPE_WMA,
	AK_AUDIO_TYPE_PCM,
	AK_AUDIO_TYPE_ADPCM_IMA,
	AK_AUDIO_TYPE_ADPCM_MS,
	AK_AUDIO_TYPE_ADPCM_FLASH,
	AK_AUDIO_TYPE_APE,
	AK_AUDIO_TYPE_FLAC,
	AK_AUDIO_TYPE_OGG_FLAC,
	AK_AUDIO_TYPE_RA8LBR,
	AK_AUDIO_TYPE_DRA,
	AK_AUDIO_TYPE_OGG_VORBIS,
	AK_AUDIO_TYPE_AC3,
	AK_AUDIO_TYPE_PCM_ALAW,
	AK_AUDIO_TYPE_PCM_ULAW,
	AK_AUDIO_TYPE_SBC,
	AK_AUDIO_TYPE_MSBC,
	AK_AUDIO_TYPE_SPEEX,
	AK_AUDIO_TYPE_SPEEX_WB
};

enum ak_audio_sample_rate 
{
	AK_AUDIO_SAMPLE_RATE_8000 = 8000,
	AK_AUDIO_SAMPLE_RATE_12000 = 12000,
	AK_AUDIO_SAMPLE_RATE_11025 = 11025,
	AK_AUDIO_SAMPLE_RATE_16000 = 16000,
	AK_AUDIO_SAMPLE_RATE_22050 = 22050,
	AK_AUDIO_SAMPLE_RATE_24000 = 24000,
	AK_AUDIO_SAMPLE_RATE_32000 = 32000,
	AK_AUDIO_SAMPLE_RATE_44100 = 44100,
	AK_AUDIO_SAMPLE_RATE_48000 = 48000,
	AK_AUDIO_SAMPLE_RATE_64000 = 64000,// now not support
};

enum ak_audio_dev_buf_size 
{
	AK_AUDIO_DEV_BUF_SIZE_256 = 256,// now not support
	AK_AUDIO_DEV_BUF_SIZE_512 = 512,
	AK_AUDIO_DEV_BUF_SIZE_1024 = 1024,
	AK_AUDIO_DEV_BUF_SIZE_2048 = 2048,
	AK_AUDIO_DEV_BUF_SIZE_3072 = 3072,
	AK_AUDIO_DEV_BUF_SIZE_4096 = 4096
};

enum ak_audio_sample_bit 
{
	AK_AUDIO_SMPLE_BIT_16 = 16
};

/* audio data attribute */
struct ak_audio_data_attr 
{
	enum ak_audio_sample_rate sample_rate;
	enum ak_audio_sample_bit sample_bits;
	enum ak_audio_channel_type channel_num;
};

struct audio_stream 
{
	unsigned char *data; 	//stream data
	unsigned int len; 		//stream len in bytes
	unsigned long long ts;	//timestamp(ms)
	unsigned long seq_no;	//current stream sequence no according to frame
};

struct ak_audio_eq_attr 
{
    //int eq_mode;
    signed short pre_gain;      //-12 <= x.xxx <= 12
	unsigned long bands;      //1~10
	unsigned long bandfreqs[EQ_MAX_BANDS];
	signed short bandgains[EQ_MAX_BANDS];  //-12 <= x.xxx <= 12
	unsigned short bandQ[EQ_MAX_BANDS];     // q < sr/(2*f)
	unsigned short band_types[EQ_MAX_BANDS];        
	unsigned char   smoothEna;   
    unsigned short smoothTime;  
    unsigned char     dcRmEna;
    unsigned long    dcfb;
    /*** for EQ aslc ***/
    unsigned char   aslc_ena;
    unsigned short  aslc_level_max;       
};

enum ak_audio_eq_type {
    TYPE_NO ,
    TYPE_HPF ,
    TYPE_LPF ,
    TYPE_HSF ,
    TYPE_LSF ,
    TYPE_PF1    //PeaKing filter
};

#endif

/* end of file */
