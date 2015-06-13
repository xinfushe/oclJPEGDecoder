#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "macro.h"
#include "jpeg.h"
#include "bitstream.h"
#include "huffman.h"
#include "zigzag.h"
#include "idct.h"

const int DEFAULT_ARY=4;
typedef HuffmanTree<DEFAULT_ARY,uint8_t> HufTree;

ZigZag<8,8> zigzag_table;

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
            // TODO: use memchr()
            for (right=left;right<tot && buffer[right]!=0xFF;right++);
            if (right<tot)
            {
                if (buffer[right+1]==0)
                    strm.append(&buffer[left],right-left+1);
                else if (buffer[right+1]==0xD9)
                {
                    if (left<right) strm.append(&buffer[left],right-left);
                    fseek(fp,right-tot,SEEK_CUR);
                    return false; // eoi
                }
                else
                    return false; // error
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
    jpg.tot_blks_per_mcu=0;
    for (int i=0;i<frame.num_channels;i++)
    {
        const int h=frame.channel_info[i].sampling_factor>>4;
        const int v=frame.channel_info[i].sampling_factor&0xF;
        jpg.mcu_width=max(jpg.mcu_width,h);
        jpg.mcu_height=max(jpg.mcu_height,v);
        jpg.blks_per_mcu[i]=h*v;
        jpg.tot_blks_per_mcu+=h*v;
    }
    jpg.mcu_width*=8;
    jpg.mcu_height*=8;
    assert(jpg.mcu_width==16 && jpg.mcu_height==16 && jpg.blks_per_mcu[0]==4 && jpg.blks_per_mcu[1]==1); // only for 4:2:0

    jpg.mcu_count_w=(frame.img_width-1)/jpg.mcu_width+1;
    jpg.mcu_count_h=(frame.img_height-1)/jpg.mcu_height+1;
    jpg.mcu_count=jpg.mcu_count_w*jpg.mcu_count_h;
    jpg.mcu_data=new coef_t[jpg.mcu_count*jpg.tot_blks_per_mcu][64];
    static_assert(sizeof(jpg.mcu_data)==sizeof(void*) && \
                  64*sizeof(coef_t)==((char*)&jpg.mcu_data[1][0]-(char*)&jpg.mcu_data[0][0]) && \
                  64*sizeof(coef_t)==sizeof(jpg.mcu_data[0]),"inappropratite type");

    printf("[ ] %d * %d = %d MCUs in total, %d blocks per MCU.\n",jpg.mcu_count_w,jpg.mcu_count_h,jpg.mcu_count,jpg.tot_blks_per_mcu);
    printf("[ ] MCU Size: %u px * %u px\n",jpg.mcu_width,jpg.mcu_height);
}

static bool decode_huffman_block(BitStream& strm, coef_t& last_dc, coef_t coef[64], const HufTree& dc, const HufTree& ac)
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
    const int& num_channels=jpg.scan_info.num_channels; // here we refer to scan_info because it's releated to huffman decoding
    coef_t *dc_coef=new coef_t[num_channels];
    memset(dc_coef,0,sizeof(coef_t)*num_channels);
    bool more_data_avail=true;
    int mcu_idx,ch_idx,blk_idx,overall_idx=0;
    for (mcu_idx=0;mcu_idx<jpg.mcu_count;mcu_idx++)
    {
        for (ch_idx=0;ch_idx<num_channels;ch_idx++)
        {
            if ((mcu_idx>0 || ch_idx>0) && strm.eof())
            {
                printf("[X] data incomplete. (%d/%d mcu)\n",mcu_idx,jpg.mcu_count);
                return false;
            }
            for (blk_idx=0;blk_idx<jpg.blks_per_mcu[ch_idx];blk_idx++)
            {
                // get more data
                if (more_data_avail)
                    more_data_avail=read_more_data(strm,fp,MIN_BUFFER_SIZE);

                // determine which huffman tree to use
                const auto dc=htree[jpg.scan_info.channel_data[ch_idx].huff_tbl_id>>4];
                const auto ac=htree[0x10|(jpg.scan_info.channel_data[ch_idx].huff_tbl_id&0xF)];
                vassert(dc!=NULL && ac!=NULL);

                coef_t mat[64]={0};
                if (!decode_huffman_block(strm,dc_coef[ch_idx],mat,*dc,*ac))
                {
                    printf("[X] data corrupted. (%d/%d mcu %d/%d ch %d/%d blk)\n",mcu_idx,jpg.mcu_count,ch_idx,num_channels,blk_idx,jpg.blks_per_mcu[ch_idx]);
                    goto corrupted;
                }
                else
                {
                    memcpy(&jpg.mcu_data[overall_idx++],mat,sizeof(mat));
                }
            }
        }
    }
    goto finished;
corrupted:

finished:
    // clean
    for (uint8_t i=0;i<32;i++)
        if (htree[i]!=NULL)
            delete htree[i];
    return mcu_idx==jpg.mcu_count;
}

template <int rows, int cols>
void zigzag_init(int *table)
{
    const int n=rows*cols;
    int cur_x=0,cur_y=0;
    int dx=1,dy=-1; // upper-right direction
    for (int i=0;i<n;i++)
    {
        table[i]=cur_y*cols+cur_x;
        if (out_of_map<rows,cols>(cur_x+dx,cur_y+dy))
        {
            if (cur_x<cols-1 && cur_y<rows-1)
            {
                if (dx>0) cur_x++;else cur_y++;
            }else
            {
                if (dx<0) cur_x++;else cur_y++;
            }
            // change direction
            dx=-dx;dy=-dy;
        }else
        {
            cur_x+=dx;
            cur_y+=dy;
        }
    }
    assert(table[n-1]==n-1);
}

bool decode_mcu_data(JPG_DATA &jpg, FILE * const fp)
{
    // define bitmap file structures
    #pragma pack(1)
    typedef struct tagBITMAPINFOHEADER{
        uint32_t      biSize;
        int32_t       biWidth;
        int32_t       biHeight;
        uint16_t       biPlanes;
        uint16_t       biBitCount;
        uint32_t      biCompression;
        uint32_t      biSizeImage;
        uint32_t       biXPelsPerMeter;
        uint32_t       biYPelsPerMeter;
        uint32_t      biClrUsed;
        uint32_t      biClrImportant;
    } BITMAPINFOHEADER, * LPBITMAPINFOHEADER,*PBITMAPINFOHEADER;

    typedef struct tagBITMAPFILEHEADER {
        uint16_t    bfType;
        uint32_t   bfSize;
        uint16_t    bfReserved1;
        uint16_t    bfReserved2;
        uint32_t   bfOffBits;
    }  BITMAPFILEHEADER,  * LPBITMAPFILEHEADER,*PBITMAPFILEHEADER;
    #pragma pack()
    // creating bmp file
    FILE *bmp=fopen("m:\\output.bmp","wb");
    BITMAPINFOHEADER bi={0};
    BITMAPFILEHEADER bf={0};
    bi.biSize=sizeof(BITMAPINFOHEADER);
	bi.biWidth=jpg.frame_info.img_width;
	bi.biHeight=-jpg.frame_info.img_height;
	bi.biPlanes=1;
	bi.biBitCount=32;
	bi.biClrUsed=0;
	bi.biClrImportant=0;
	bi.biCompression=0; // BI_RGB
	bf.bfType=0x4d42;
	bf.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bi.biWidth*abs(bi.biHeight)*4;
	bf.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	assert(bf.bfOffBits==54);
	fwrite(&bf,sizeof(bf),1,bmp);
	fwrite(&bi,sizeof(bi),1,bmp);
    // allocating memory space
    uint32_t **mcu_scanline=new uint32_t*[jpg.mcu_height];
    for (int i=0;i<jpg.mcu_height;i++)
    {
        mcu_scanline[i]=new uint32_t[jpg.mcu_width*jpg.mcu_count_w]; // possibly larger than real width
    }
    coef_t (*mat)[64]=new coef_t[jpg.tot_blks_per_mcu][64];
    // initializing
    const int sample_Y_h=jpg.frame_info.channel_info[0].sampling_factor>>4;
    const int sample_Y_v=jpg.frame_info.channel_info[0].sampling_factor&0xF;
    const int sample_U_h=jpg.frame_info.channel_info[1].sampling_factor>>4;
    const int sample_U_v=jpg.frame_info.channel_info[1].sampling_factor&0xF;
    const int sample_V_h=jpg.frame_info.channel_info[2].sampling_factor>>4;
    const int sample_V_v=jpg.frame_info.channel_info[2].sampling_factor&0xF;
    // iterating MCUs
    int overall_idx=0;
    for (int my=0;my<jpg.mcu_count_h;my++)
    {
        for (int mx=0;mx<jpg.mcu_count_w;mx++)
        {
            int block_idx=0;
            for (int ch=0;ch<jpg.frame_info.num_channels;ch++)
            {
                const coef_t * const qt=jpg.quantization_table[jpg.frame_info.channel_info[ch].quant_tbl_id];
                for (int k=0;k<jpg.blks_per_mcu[ch];k++)
                {
                    for (int pos=0;pos<64;pos++)
                    {
                        mat[block_idx][zigzag_table[pos]]=jpg.mcu_data[overall_idx][pos]*qt[pos]; // zig-zag & inverse quantizatize
                    }
                    Fast_IDCT(mat[block_idx]);
                    block_idx++;
                    overall_idx++;
                }
            }
            // perform color space conversion block by block
            assert(jpg.blks_per_mcu[1]==1 && jpg.blks_per_mcu[2]==1); // the following code only works in ?:1:1 mode
            for (int y=0;y<jpg.mcu_height;y++)
            {
                for (int x=0;x<jpg.mcu_width;x++)
                {
                    int Y=mat[(y>>3)*sample_Y_h+(x>>3)][((y&7)<<3)|(x&7)];
                    int U=mat[jpg.blks_per_mcu[0]][((y>>1)<<3)|(x>>1)];
                    int V=mat[jpg.blks_per_mcu[0]+1][((y>>1)<<3)|(x>>1)];
                    mcu_scanline[y][x+mx*jpg.mcu_width]=RGBClamp32(Y+1.402*V+128,Y-0.34414*U-0.71414*V+128,Y+1.772*U+128);
                }
            }
        }
        // write scanline
        for (int i=0;i<jpg.mcu_height;i++)
            fwrite(mcu_scanline[i],sizeof(uint32_t),jpg.frame_info.img_width,bmp);
    }
    fclose(bmp);
    return true;
}
