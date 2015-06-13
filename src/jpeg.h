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
    const char *codeword[256]; // at most 256 codewords
    uint8_t value[256];
};

typedef int coef_t;

struct JPG_DATA
{
    APP0 app0;
    coef_t *quantization_table[4];
    SOF0 frame_info;
    HUFFMAN_TABLE *huffman_table[32];
    void *thumbnail;
    SOS scan_info;

    int mcu_width; // in pixels
    int mcu_height; // in pixels
    int mcu_count_w;
    int mcu_count_h;
    int mcu_count;
    coef_t (*mcu_data)[64];

    int blks_per_mcu[4]; // Color Component Blocks per MCU
    int tot_blks_per_mcu;

};
