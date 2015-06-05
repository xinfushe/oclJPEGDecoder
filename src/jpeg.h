// JPEG Standard Definiton

#pragma pack(1)
struct APP0
{
    uint16_t len;
    uint8_t id[5]; // JFIF\0
    uint16_t ver; // typically 0x0102
    uint8_t res_unit;
    uint16_t res_x;
    uint16_t res_y;
    uint8_t thumbnail_width;
    uint8_t thumbnail_height;
    uint8_t thumbnail_data[0][3];
};

struct SOF0
{
    uint8_t bit_depth;
    uint16_t img_height;
    uint16_t img_width;
    uint8_t num_channels;
    struct
    {
        uint8_t id;
        uint8_t sampling_factor; // bit 0~3: vertical; bit 4~7: horizontal
        uint8_t quant_tbl_id;
    }channel_info[3];
};

struct SOS
{
    uint8_t num_channels;
    struct
    {
        uint8_t id;
        uint8_t huff_tbl_id; // bit 0~3: AC; bit 4~7: DC
    }channel_data[3];
    uint8_t reserved[3]; // 0x00, 0x3F, 0x00
};

#pragma pack()

// Application-Defined Structres
struct HUFFMAN_TABLE
{
    int num_codeword;
    char codeword[256][20]; // max length of a codeword is 16
    uint8_t value[256];
};

enum SAMPLING_TYPE
{
    YUV444,
    YUV422_H2V1,
    YUV422_H1V2,
    YUV420,
    UNKNOWN
};

struct JPG_DATA
{
    APP0 app0;
    int *quantization_table[4];
    SOF0 frame_info;
    HUFFMAN_TABLE *huffman_table[32];
    void *thumbnail;
    SOS scan_info;
};
