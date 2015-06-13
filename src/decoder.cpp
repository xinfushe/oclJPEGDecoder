#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "macro.h"
#include "jpeg.h"
#include "bitstream.h"
#include "huffman.h"

const int DEFAULT_ARY=4;
typedef HuffmanTree<DEFAULT_ARY,uint8_t> HufTree;

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
        const uint8_t acid=jpg.scan_info.channel_data[i].huff_tbl_id&0xF;
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
        puts("[X] sorry, currently only supports 8-bit YUV 4:2:0 format");
        return false;
    }
    return true;
}

static int inline convert_number(int value, const uint8_t nbits)
{
    if (!(value>>(nbits-1))) // sign bit
    {
        // negative
        ++value;
        value-=1<<nbits;
        vassert(value!=0);
    }
    return value;
}

static int inline read_number(BitStream& strm, const uint8_t bits)
{
    if (bits)
    {
        int value=strm.nextBits(bits);
        return convert_number(value,bits);
    }else
    return 0;
}

static bool read_more_data(BitStream& strm, FILE * const fp, const size_t buffer_size)
{
    if (strm.getSize()<buffer_size)
    {
        uint8_t buffer[buffer_size+1];
        const size_t tot=fread(buffer,1,buffer_size,fp);

        for (size_t left=0;left<tot;left++)
        {
            size_t right;
            for (right=left;right<tot && buffer[right]!=0xFF;right++);
            if (right<tot)
            {
                if (buffer[right+1]==0)
                    strm.append(&buffer[left],right-left+1);
                else if (buffer[right+1]==0xD9)
                {
                    if (left<right) strm.append(&buffer[left],right-left);
                    fseek(fp,right-tot,SEEK_CUR);
                    break;
                }
                else
                    return false;
            }else
            {
                strm.append(&buffer[left],tot-left);
                break;
            }
            left=right+1;
        }
    }
    return true;
}

void decode_init(JPG_DATA &jpg)
{
    const SOF0 &frame=jpg.frame_info;
    jpg.mcu_width=0;
    jpg.mcu_height=0;
    for (int i=0;i<frame.num_channels;i++)
    {
        const int h=frame.channel_info[i].sampling_factor>>4;
        const int v=frame.channel_info[i].sampling_factor&0xF;
        jpg.mcu_width=max(jpg.mcu_width,h);
        jpg.mcu_height=max(jpg.mcu_height,v);
        jpg.blks_per_mcu[i]=h*v;
    }
    jpg.mcu_width*=8;
    jpg.mcu_height*=8;
    assert(jpg.mcu_width==16 && jpg.mcu_height==16 && jpg.blks_per_mcu[0]==4 && jpg.blks_per_mcu[1]==1); // only for 4:2:0

    jpg.mcu_count_w=(frame.img_width-1)/jpg.mcu_width+1;
    jpg.mcu_count_h=(frame.img_height-1)/jpg.mcu_height+1;
    jpg.num_mcu=jpg.mcu_count_w*jpg.mcu_count_h;

    printf("[ ] %d * %d = %d MCUs in total\n",jpg.mcu_count_w,jpg.mcu_count_h,jpg.num_mcu);
    printf("[ ] MCU Size: %u px * %u px\n",jpg.mcu_width,jpg.mcu_height);
}

static bool decode_block(BitStream& strm, int& last_dc, int coef[64], const HufTree& dc, const HufTree& ac)
{
    int count=0;
    int value;
    // read in dc component
    const auto hnode=dc.findCode(strm);
    if (hnode==NULL) return false;

    const auto hval=*hnode;
    assert(hval<=25);
    value=read_number(strm,hval);

    coef[count++]=last_dc+=value;

    // read in 63 ac components
    while (count<64)
    {
        const auto hnode=ac.findCode(strm);
        if (hnode==NULL) return false;

        const int num_leading_0=(*hnode)>>4;
        const uint8_t len_val=(*hnode)&0xF;

        count+=num_leading_0; // skip consecutive zeroes
        assert(count+1<=64);

        if (len_val==0) // value is 0 in this case
        {
            if (num_leading_0==0)
                break;
            else
                count++;
        }else // value is non-zero
        {
            int value=read_number(strm,len_val);
            coef[count++]=value;
        }
    }
    return count<=64;
}

bool decode_huffman_data(JPG_DATA &jpg, FILE * const fp)
{
    const size_t MIN_BUFFER_SIZE=512;
//    FILE *log = fopen("m:\\my.txt","wt");
//    static int i;

    // create huffman trees
    HufTree* htree[32]={NULL};
    for (uint8_t i=0;i<32;i++)
        if (jpg.huffman_table[i]!=NULL)
        {
            htree[i]=new HufTree(jpg.huffman_table[i]->codeword,jpg.huffman_table[i]->value,jpg.huffman_table[i]->num_codeword);
        }
    // initialization
    decode_init(jpg);
    // now we can start
    BitStream strm(1536);
    const int& num_channels=jpg.frame_info.num_channels;
    int *dc_coef=new int[num_channels];
    memset(dc_coef,0,sizeof(int)*num_channels);
    int mcu_idx,ch_idx,blk_idx;
    for (mcu_idx=0;mcu_idx<jpg.num_mcu;mcu_idx++)
    {
        for (ch_idx=0;ch_idx<num_channels;ch_idx++)
        {
            if (mcu_idx>0 && strm.eof())
            {
                printf("[X] data incomplete. (%d/%d mcu)\n",mcu_idx,jpg.num_mcu);
                return false;
            }
            for (blk_idx=0;blk_idx<jpg.blks_per_mcu[ch_idx];blk_idx++)
            {
                // get more data
                read_more_data(strm,fp,MIN_BUFFER_SIZE);

                // determine which huffman tree to use
                const auto dc=htree[jpg.scan_info.channel_data[ch_idx].huff_tbl_id>>4];
                const auto ac=htree[0x10|(jpg.scan_info.channel_data[ch_idx].huff_tbl_id&0xF)];
                vassert(dc!=NULL && ac!=NULL);

                int mat[64]={0};
                if (!decode_block(strm,dc_coef[ch_idx],mat,*dc,*ac))
                {
                    printf("[X] data corrupted. (%d/%d mcu %d/%d ch %d/%d blk)\n",mcu_idx,jpg.num_mcu,ch_idx,num_channels,blk_idx,jpg.blks_per_mcu[ch_idx]);
                    goto corrupted;
                }
                else
                {
                    //printf("[] === (%d/%d mcu %d/%d ch %d/%d blk) ===\n",mcu_idx,jpg.num_mcu,ch_idx,num_channels,blk_idx,jpg.blks_per_mcu[ch_idx]);
//                    ++i;
//                    fprintf(log,"block %d\n",i);
//                    for (int k=0;k<64;k++)
//                    {
//                        //printf("%d,",mat[k]);
//                        fprintf(log,"%d,",mat[k]);
//                    }
//                    fputs("\n",log);
                    //puts("");
                }
            }
        }
    }
    goto finished;
corrupted:

finished:
//    fclose(log);
    // clean
    for (uint8_t i=0;i<32;i++)
        if (htree[i]!=NULL)
            delete htree[i];
    return mcu_idx==jpg.num_mcu;
}
