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

static int inline convert_number(int value, const uint8_t len)
{
    if (!(value>>(len-1))) // sign bit
    {
        // negative
        ++value;
        value-=1<<len;
    }
    return value;
}

bool decode_huffman_data(JPG_DATA &j, FILE * const fp)
{
    const int DEFAULT_ARY=4;
    const size_t MIN_BUFFER_SIZE=512;
    // create huffman trees
    HuffmanTree<DEFAULT_ARY,uint8_t>* htree[32]={NULL};
    for (uint8_t i=0;i<32;i++)
    {
        if (j.huffman_table[i]!=NULL)
        {
            htree[i]=new HuffmanTree<DEFAULT_ARY,uint8_t>(j.huffman_table[i]->codeword,j.huffman_table[i]->value,j.huffman_table[i]->num_codeword);
        }
    }
    // prepare
    j.mcu_width=0;
    j.mcu_height=0;
    j.yblks_per_mcu=0;
    j.blks_per_mcu=0;
    for (int i=0;i<j.frame_info.num_channels;i++)
    {
        const int h=j.frame_info.channel_info[i].sampling_factor>>4;
        const int v=j.frame_info.channel_info[i].sampling_factor&0xF;
        j.mcu_width=max(j.mcu_width,h);
        j.mcu_height=max(j.mcu_height,v);
        j.blks_per_mcu+=h*v;
        if (i==0)
            j.yblks_per_mcu=h*v;
        else
            assert(h*v==1);
    }
    j.mcu_width*=8;
    j.mcu_height*=8;
    assert(j.mcu_width==16 && j.mcu_height==16 && j.blks_per_mcu==6 && j.yblks_per_mcu==4); // only for 4:2:0

    j.mcu_count_w=(j.frame_info.img_width-1)/j.mcu_width+1;
    j.mcu_count_h=(j.frame_info.img_height-1)/j.mcu_height+1;
    j.num_mcu=j.mcu_count_w*j.mcu_count_h;

    printf("[ ] %d * %d = %d MCUs in total\n",j.mcu_count_w,j.mcu_count_h,j.num_mcu);
    printf("[ ] MCU Size: %u px * %u px\n",j.mcu_width,j.mcu_height);
    // now we can start
    BitStream strm(1024);
    int dccoef[4]={0};
    int n;
    for (n=0;n<j.num_mcu;n++)
    {
        if (n>0 && strm.eof())
        {
            printf("[X] data incomplete. (%d/%d mcu)\n",n,j.num_mcu);
            return false;
        }
        for (int blk=0;blk<j.blks_per_mcu;blk++)
        {
            const HuffmanTree<DEFAULT_ARY,uint8_t> *ac,*dc;
            const int ch=(blk<j.yblks_per_mcu)?0:blk-j.yblks_per_mcu+1;
            int ncoeff=0;
            vassert(ch>=0 && ch<(int)COUNT_OF(dccoef));
            dc=htree[j.scan_info.channel_data[ch].huff_tbl_id>>4];
            ac=htree[0x10|(j.scan_info.channel_data[ch].huff_tbl_id&0xF)];
            vassert(dc!=NULL && ac!=NULL);

            // get more data
            if (strm.getSize()<MIN_BUFFER_SIZE)
            {
                uint8_t tmpBuffer[MIN_BUFFER_SIZE+1];
                size_t cnt=fread(tmpBuffer,1,MIN_BUFFER_SIZE,fp);
                assert(cnt>0);

                for (size_t i=0;i<cnt;i++)
                {
                    size_t j;
                    for (j=i;j<cnt && tmpBuffer[j]!=0xFF;j++);
                    if (j<cnt)
                    {
                        if (tmpBuffer[j+1]==0)
                            strm.append(&tmpBuffer[i],j-i+1);
                        else if (tmpBuffer[j+1]==0xD9)
                        {
                            fseek(fp,j-cnt,SEEK_CUR);
                            break;
                        }
                        else
                            return false;
                    }else
                    {
                        strm.append(&tmpBuffer[i],cnt-i);
                    }
                    i=j+1;
                }
            }
            // read in dc component
            const auto node=dc->findCode(strm);
            if (node==NULL)
            {
corrupted:
                printf("[X] data corrupted. (%d/%d mcu %d/%d blk #%d coef)\n",n,j.num_mcu,blk,j.blks_per_mcu,ncoeff);
                return false;
            }
            uint32_t data=node->getData();
            assert(data<=25);
            if (data)
            {

                int value=strm.nextBits(data);
                value=convert_number(value,data);
                dccoef[ch]+=value;
            }
            int mat[64]={0};
            mat[ncoeff++]=dccoef[ch];
            // read in 63 ac components
            while (ncoeff<64)
            {
                const auto node=ac->findCode(strm);
                if (node==NULL) goto corrupted;
                uint32_t data=*node;
                const int num0=data>>4;
                const uint8_t valLen=data&0xF;
                ncoeff+=num0; // skip consecutive zeroes
                assert(ncoeff+1<=64);
                if (valLen==0)
                {
                    if (num0==0)
                        break;
                    else
                        ncoeff++;
                }else
                {
                    int value=strm.nextBits(valLen);
                    value=convert_number(value,valLen);
                    mat[ncoeff++]=value;
                    assert(value!=0);
                }
            }

/*             printf("[] === (%d/%d mcu %d/%d blk) ===\n",n,j.num_mcu,blk,j.blks_per_mcu);
 *             for (int k=0;k<64;k++)
 *                 printf("%d,",mat[k]);
 *             puts("");
 */
            assert(ncoeff<=64);
            assert(!strm.eof());
        }
    }
    // clean
    for (uint8_t i=0;i<32;i++)
        if (htree[i]!=NULL)
            delete htree[i];
    return n==j.num_mcu;
}
