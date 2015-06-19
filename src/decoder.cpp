#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "macro.h"
#include "jpeg.h"
#include "bmp.h"
#include "bitstream.h"
#include "huffman.h"
#include "zigzag.h"
#include "idct.h"

//#define USE_CPU_ONLY

const int DEFAULT_ARY=16;
typedef HuffmanTree<DEFAULT_ARY,uint8_t> HufTree;

ZigZag<8,8> zigzag_table;

bool is_supported_file(const JPG_DATA &jpg)
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
    if (jpg.frame_info.channel_info[0].sampling_factor==0x22 && \
        jpg.frame_info.channel_info[1].sampling_factor==0x11 && \
        jpg.frame_info.channel_info[2].sampling_factor==0x11)
        return true;

    if (jpg.frame_info.channel_info[0].sampling_factor==0x11 && \
        jpg.frame_info.channel_info[1].sampling_factor==0x11 && \
        jpg.frame_info.channel_info[2].sampling_factor==0x11)
        return true;

    puts("[X] sorry, currently only supports 8-bit YUV 4:2:0 or 4:4:4 format");
    return false;
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

template <size_t buffer_size>
static bool read_more_data(BitStream& strm, FILE * const fp)
{
    if (strm.getSize()<buffer_size)
    {
        uint8_t buffer[buffer_size+1];
        const size_t tot=fread(buffer,1,buffer_size,fp);

        // for convenience
        buffer[tot]=0xFF;
        // find all 0xFFs in the buffer
        for (size_t left=0;left<tot;left++)
        {
            const uint8_t* next_ff=(uint8_t*)memchr(&buffer[left],0xFF,tot-left);
            if (next_ff!=NULL)
            {
check_again:
                const size_t right=next_ff-&buffer[0];
                switch (*(next_ff+1))
                {
                case 0: // produce a 0xFF byte
                    strm.append(&buffer[left],next_ff+1);
                    left=right+1;
                    break;
                case 0xD9: // EOI
                    strm.append(&buffer[left],next_ff);
                    fseek(fp,right-tot,SEEK_CUR);
                    return false;
                case 0xFF: // need to read another byte
                    strm.append(&buffer[left],next_ff);
                    left=right; // next iteration will start at next_ff+1
                    if (right==tot-1)
                    {
                        // expect one more byte from file
                        left=tot-1;
                        next_ff=(uint8_t*)&buffer[tot-1];
                        if (1==fread(&buffer[tot],1,1,fp))
                            goto check_again;
                        else
                            return false;
                    }
                    break;
                default:
                    return false; // error
                }
            }else
            {
                // no 0xFF occurred
                strm.append(&buffer[left],tot-left);
                break;
            }
        }
    }
    return true;
}

bool decode_init(JPG_DATA &jpg)
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

    jpg.color_space=Other;
    if (jpg.frame_info.channel_info[0].sampling_factor==0x22 && \
        jpg.frame_info.channel_info[1].sampling_factor==0x11 && \
        jpg.frame_info.channel_info[2].sampling_factor==0x11)
        jpg.color_space=YUV420;
    else if (jpg.frame_info.channel_info[0].sampling_factor==0x11 && \
        jpg.frame_info.channel_info[1].sampling_factor==0x11 && \
        jpg.frame_info.channel_info[2].sampling_factor==0x11)
        jpg.color_space=YUV444;

    jpg.mcu_count_w=(frame.img_width-1)/jpg.mcu_width+1;
    jpg.mcu_count_h=(frame.img_height-1)/jpg.mcu_height+1;
    jpg.mcu_count=jpg.mcu_count_w*jpg.mcu_count_h;
    jpg.blk_count=jpg.tot_blks_per_mcu*jpg.mcu_count;
    jpg.mcu_data=new coef_t[jpg.mcu_count*jpg.tot_blks_per_mcu][64];
    static_assert(sizeof(jpg.mcu_data)==sizeof(void*) && \
                  64*sizeof(coef_t)==((char*)&jpg.mcu_data[1][0]-(char*)&jpg.mcu_data[0][0]) && \
                  64*sizeof(coef_t)==sizeof(jpg.mcu_data[0]),"inappropratite type");

    printf("[ ] %d * %d = %d MCUs in total, %d blocks per MCU.\n",jpg.mcu_count_w,jpg.mcu_count_h,jpg.mcu_count,jpg.tot_blks_per_mcu);
    printf("[ ] %d blocks in total.\n",jpg.blk_count);
    printf("[ ] MCU Size: %u px * %u px\n",jpg.mcu_width,jpg.mcu_height);

    #ifndef USE_CPU_ONLY
        puts("[C] clidct_create()");
        if (!clidct_create()) return false;

        puts("[C] clidct_allocate_memory()");
        if (!clidct_allocate_memory(jpg.blk_count,jpg.frame_info.img_width,jpg.frame_info.img_height,jpg.mcu_width,jpg.mcu_height)) return false;

        // build cl program
        puts("[C] clidct_build()");
        if (!clidct_build(jpg.color_space))
        {
            puts("[X] fatal error: failed to build opencl program. check the source code.");
            return false;
        }
    #endif

    return true;
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
    const size_t MIN_BUFFER_SIZE=2048;
    // create huffman trees
    HufTree* htree[32]={NULL};
    for (uint8_t i=0;i<32;i++)
        if (jpg.huffman_table[i]!=NULL)
        {
            htree[i]=new HufTree(jpg.huffman_table[i]->codeword,jpg.huffman_table[i]->value,jpg.huffman_table[i]->num_codeword);
        }
    // now we can start
    BitStream strm(MIN_BUFFER_SIZE*4);
    const int& num_channels=jpg.scan_info.num_channels; // here we refer to scan_info because it's releated to huffman decoding
    coef_t *dc_coef=new coef_t[num_channels];
    memset(dc_coef,0,sizeof(coef_t)*num_channels);
    bool more_data_avail=true;
    int mcu_idx,ch_idx,blk_idx,overall_block_idx=0;
    for (mcu_idx=0;mcu_idx<jpg.mcu_count;mcu_idx++)
    {
        for (ch_idx=0;ch_idx<num_channels;ch_idx++)
        {
            if ((mcu_idx>0 || ch_idx>0) && strm.eof())
            {
                printf("[X] data incomplete or buffer too small. (%d/%d mcu)\n",mcu_idx,jpg.mcu_count);
                return false;
            }
            const coef_t * const qt=jpg.quantization_table[jpg.frame_info.channel_info[ch_idx].quant_tbl_id];
            for (blk_idx=0;blk_idx<jpg.blks_per_mcu[ch_idx];blk_idx++)
            {
                // get more data
                if (more_data_avail)
                    more_data_avail=read_more_data<MIN_BUFFER_SIZE>(strm,fp);

                // determine which huffman tree to use
                const auto dc=htree[jpg.scan_info.channel_data[ch_idx].huff_tbl_id>>4];
                const auto ac=htree[0x10|(jpg.scan_info.channel_data[ch_idx].huff_tbl_id&0xF)];
                vassert(dc!=NULL && ac!=NULL);

                coef_t mat[64]={0};
                coef_t zziq_mat[64];
                if (!decode_huffman_block(strm,dc_coef[ch_idx],mat,*dc,*ac))
                {
                    printf("[X] data corrupted. (%d/%d mcu %d/%d ch %d/%d blk)\n",mcu_idx,jpg.mcu_count,ch_idx,num_channels,blk_idx,jpg.blks_per_mcu[ch_idx]);
                    goto corrupted;
                }
                else
                {
                    for (int pos=0;pos<64;pos++)
                    {
                        zziq_mat[zigzag_table[pos]]=mat[pos]*qt[pos]; // zig-zag & inverse quantizatize
                    }
                    memcpy(&jpg.mcu_data[overall_block_idx++],zziq_mat,sizeof(zziq_mat));
                }
            }
        }
    }
    goto finished;
corrupted:

    goto cleanup;
finished:
    #ifndef USE_CPU_ONLY
        puts("[C] clidct_send()");
        clock_t timestamp;
        timestamp=clock();
        clidct_transfer_data_to_device(jpg.mcu_data,0,jpg.blk_count);
        printf("Time elapsed for writing data to device: %ld\n",clock()-timestamp);
    #endif
cleanup:
    // clean
    for (uint8_t i=0;i<32;i++)
        if (htree[i]!=NULL)
            delete htree[i];
    return mcu_idx==jpg.mcu_count;
}

uint32_t YUV_to_RGB32(coef_t Y, coef_t U, coef_t V)
{
    return RGBClamp32((int)(Y+1.402*V+128),(int)(Y-0.34414*U-0.71414*V+128),(int)(Y+1.772*U+128));
}

FILE* bmp_create(const char* path, const int width, const int height)
{
    FILE *bmp=fopen(path,"wb");
    if (bmp!=NULL)
    {
        BITMAPINFOHEADER bi={0};
        BITMAPFILEHEADER bf={0};
        bi.biSize=sizeof(BITMAPINFOHEADER);
        bi.biWidth=width;
        bi.biHeight=-height;
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
    }
	return bmp;
}

bool decode_mcu_data(JPG_DATA &jpg, FILE * const fp)
{
    clock_t timestamp;
    char* image_data=NULL;
    const size_t image_size=(size_t)jpg.frame_info.img_width*(size_t)jpg.frame_info.img_height*4;
    #ifndef USE_CPU_ONLY
        // run IDCT on GPU
        timestamp=clock();
        puts("[C] clidct_run()");
        if (!clidct_run(jpg.color_space)) return false;
        puts("[C] clidct_wait()");
        if (!clidct_wait_for_completion()) return false;
        printf("Time elapsed for running the IDCT kernel: %ld\n",clock()-timestamp);
        // retrieve output (transformed blocks)
        timestamp=clock();
        puts("[C] clidct_recv()");
        image_data=new char[image_size];
        if (!clidct_retrieve_image_from_device(image_data,jpg.frame_info.img_width,jpg.frame_info.img_height)) return false;
        // if (!clidct_retrieve_data_from_device(jpg.mcu_data)) return false;
        printf("Time elapsed for reading data from device: %ld\n",clock()-timestamp);
    #endif

    // creating bmp file
    FILE *bmp=bmp_create("m:\\output.bmp",jpg.frame_info.img_width,jpg.frame_info.img_height);
    // allocating memory
    uint32_t **mcu_scanline=new uint32_t*[jpg.mcu_height];
    for (int i=0;i<jpg.mcu_height;i++)
    {
        mcu_scanline[i]=new uint32_t[jpg.mcu_width*jpg.mcu_count_w]; // possibly larger than real width
    }
    /*const*/ coef_t (*mat)[64]=NULL;
    int overall_block_idx=0;
    #ifdef USE_CPU_ONLY
        // initializing
        const int sample_Y_h=jpg.frame_info.channel_info[0].sampling_factor>>4;
        const int sample_Y_v=jpg.frame_info.channel_info[0].sampling_factor&0xF;
        const int sample_Y_n=sample_Y_h*sample_Y_v;
        const int sample_U_h=jpg.frame_info.channel_info[1].sampling_factor>>4;
        const int sample_U_v=jpg.frame_info.channel_info[1].sampling_factor&0xF;
        const int sample_V_h=jpg.frame_info.channel_info[2].sampling_factor>>4;
        const int sample_V_v=jpg.frame_info.channel_info[2].sampling_factor&0xF;
        const int sample_YU_h=sample_Y_h/sample_U_h;
        const int sample_YU_v=sample_Y_v/sample_U_v;
        const int sample_YV_h=sample_Y_h/sample_V_h;
        const int sample_YV_v=sample_Y_v/sample_V_v;
        // iterating through MCUs
        for (int my=0;my<jpg.mcu_count_h;my++)
        {
            for (int mx=0;mx<jpg.mcu_count_w;mx++)
            {
                mat=&jpg.mcu_data[overall_block_idx];
                for (int blk=0;blk<jpg.tot_blks_per_mcu;blk++)
                {
                    Fast_IDCT(mat[blk]);
                    overall_block_idx++;
                }
                // perform color space conversion block by block
                if (jpg.blks_per_mcu[1]==1 && jpg.blks_per_mcu[2]==1)
                {
                    // WARNING: the following code only works in ?:1:1 mode
                    if (jpg.blks_per_mcu[0]==1)
                    {
                        assert(jpg.mcu_width==8 && jpg.mcu_height==8 && sample_Y_n==1);
                        int pos=0;
                        for (int y=0;y<jpg.mcu_height;y++)
                        {
                            for (int x=0;x<jpg.mcu_width;x++)
                            {
                                int Y=mat[0][pos];
                                int U=mat[1][pos];
                                int V=mat[2][pos];
                                mcu_scanline[y][x+(mx<<3)]=YUV_to_RGB32(Y,U,V);
                                pos++;
                            }
                        }
                    }else
                    {
                        for (int y=0;y<jpg.mcu_height;y++)
                        {
                            for (int x=0;x<jpg.mcu_width;x++)
                            {
                                int Y=mat[(y>>3)*sample_Y_h+(x>>3)][((y&7)<<3)|(x&7)];
                                int U=mat[sample_Y_n][((y/sample_YU_v)<<3)+x/sample_YU_h];
                                int V=mat[sample_Y_n+1][((y/sample_YV_v)<<3)+x/sample_YV_h];
                                mcu_scanline[y][x+mx*jpg.mcu_width]=YUV_to_RGB32(Y,U,V);
                            }
                        }
                    }

                }else
                {
                    printf("[X] Unsupported color space.\n");
                    goto failed;
                }
            }
            // write scanline
            for (int i=0;i<jpg.mcu_height;i++)
                fwrite(mcu_scanline[i],sizeof(uint32_t),jpg.frame_info.img_width,bmp);
        }
    #else
        // GPU Accelerated
        if (1!=fwrite(image_data,image_size,1,bmp))
        {
            puts("[X] Write file error");
            goto failed;
        }
        overall_block_idx=jpg.blk_count;
    #endif // USE_CPU_ONLY
    goto finished;
failed:

    goto cleanup;
finished:

cleanup:
    // clean
    fclose(bmp);
    for (int i=0;i<jpg.mcu_height;i++)
        delete[] mcu_scanline[i];
    delete[] mcu_scanline;
    if (image_data) delete[] image_data;
    #ifndef USE_CPU_ONLY
        puts("[C] clidct_clean_up()");
        clidct_clean_up();
    #endif
    return overall_block_idx==jpg.blk_count;
}
