#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "macro.h"
#include "jpeg.h"
#include "bitstream.h"
#include "huffman.h"

bool is_supported_file(JPG_DATA &jpg)
{
    if (jpg.frame_info.bit_depth!=8)
    {
        puts("[X] unsupported bit depth");
        return false;
    }
    if (jpg.frame_info.num_channels!=3 || jpg.scan_info.num_channels!=3)
    {
        puts("[X] unsupported number of components");
        return false;
    }
    if (jpg.frame_info.img_width<=0 || jpg.frame_info.img_height<=0)
    {
        puts("[X] invalid dimensions");
        return false;
    }
    for (uint8_t i=0;i<jpg.frame_info.num_channels;i++)
    {
        if (NULL==jpg.quantization_table[jpg.frame_info.channel_info[i].quant_tbl_id])
        {
            puts("[X] corrupted file. missing quantization table.");
            return false;
        }
    }
    for (uint8_t i=0;i<jpg.scan_info.num_channels;i++)
    {
        const uint8_t acid=jpg.scan_info.channel_data[i].huff_tbl_id&7;
        const uint8_t dcid=jpg.scan_info.channel_data[i].huff_tbl_id>>4;
        if (NULL==jpg.huffman_table[dcid])
        {
            puts("[X] corrupted file. missing huffman table for DC component.");
            return false;
        }
        if (NULL==jpg.huffman_table[acid|0x10])
        {
            puts("[X] corrupted file. missing huffman table for AC component.");
            return false;
        }
    }
    if (jpg.frame_info.channel_info[0].sampling_factor!=0x22 || \
        jpg.frame_info.channel_info[1].sampling_factor!=0x11 || \
        jpg.frame_info.channel_info[2].sampling_factor!=0x11)
    {
        puts("[X] sorry, currently only 4:2:0 is supported");
        return false;
    }
    return true;
}

bool decode_huffman_data(JPG_DATA &jpg, FILE * const strm)
{
    // create huffman trees
    HuffmanTree<4,uint8_t>* htree[32]={NULL};
    for (uint8_t i=0;i<32;i++)
    {
        if (jpg.huffman_table[i]!=NULL)
            htree[i]=new HuffmanTree<4,uint8_t>(jpg.huffman_table[i]->codeword,jpg.huffman_table[i]->value,jpg.huffman_table[i]->num_codeword);
    }
    return false;
}
