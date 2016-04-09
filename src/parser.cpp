#include "stdafx.h"

#include "macro.h"
#include "jpeg.h"
#include "decoder.h"

bool read_soi(JPG_DATA &jpg, FILE * const strm)
{
    uint8_t tag[2];
    if (1==fread(tag,sizeof(tag),1,strm) && tag[0]==0xFF && tag[1]==0xD8)
        return true;
    else
    {
        puts("[X] SOI is missing or broken.");
        return false;
    }
}

bool read_app0(JPG_DATA &jpg, FILE * const strm)
{
    if (1!=fread(&jpg.app0,sizeof(APP0),1,strm))
    {
        puts("[X] APP0 is incomplete.");
        return false;
    }
    // calculate the size of thumbnail image
    const size_t tn_size=(uint16_t)jpg.app0.thumbnail_width*(uint16_t)jpg.app0.thumbnail_height;
    // validate
    if (memcmp(jpg.app0.id,"JFIF\0",5) || bswap16(jpg.app0.len)!=sizeof(APP0)+tn_size)
    {
        puts("[X] APP0 is broken.");
        return false;
    }
    // read thumbnail image
    if (tn_size)
    {
        jpg.thumbnail=new uint8_t[tn_size];
        if (1!=fread(jpg.thumbnail,1,tn_size,strm))
        {
            puts("[X] Thumbnail image is broken.");
            return false;
        }
    }
    return true;
}

bool read_dqt(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    uint8_t byte;
    while (len>0)
    {
        fread(&byte,sizeof(byte),1,strm);
        const uint8_t prec=byte>>4;
        const uint8_t id=byte&0xF;
        if (id>3 || jpg.quantization_table[id]!=NULL)
        {
            printf("[X] Quantization table #%u is invalid or already defined.\n",id);
            return false;
        }
        if (prec!=1 && prec!=0)
        {
            printf("[X] Invalid Precision Value for Quantization Table #%u.\n",id);
            return false;
        }
        jpg.quantization_table[id]=new coef_t[64];
        uint8_t qt8[64];
        uint16_t qt16[64];
        switch (prec)
        {
        case 0: // 8-bit
            if (1==fread(qt8,sizeof(qt8),1,strm))
            {
                for (size_t i=0;i<64;i++)
                    jpg.quantization_table[id][i]=qt8[i];
            }else
            {
corrupted:
                printf("[X] Quantization Table #%u is corrupted.\n",id);
                return false;
            }
            break;
        case 1: // 16-bit
            if (1==fread(qt16,sizeof(qt16),1,strm))
            {
                for (size_t i=0;i<64;i++)
                    jpg.quantization_table[id][i]=qt16[i];
            }else goto corrupted;
            break;
        }
        if (len>=64*(size_t)(prec+1)+1)
            len-=64*(size_t)(prec+1)+1;
        else
        {
            puts("[X] DQT is corrupted.");
            return false;
        }
        printf("[ ] --- Quantization Table #%u (%d-bit)\n",id,8<<prec);
    }
    return true;
}

bool read_sof(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    if (len!=sizeof(SOF0) || 1!=fread(&jpg.frame_info,sizeof(SOF0),1,strm))
    {
        puts("[X] SOF0 is corrupted.");
        return false;
    }else if (jpg.frame_info.num_channels!=3 || jpg.frame_info.bit_depth!=8)
    {
        puts("[X] Unsupported Sampling");
        return false;
    }else
    {
        // fix endianess
        jpg.frame_info.img_width=bswap16(jpg.frame_info.img_width);
        jpg.frame_info.img_height=bswap16(jpg.frame_info.img_height);

        printf("Dimensions: %u px * %u px\n",jpg.frame_info.img_width,jpg.frame_info.img_height);
        printf("Bit Depth: %u\n",jpg.frame_info.bit_depth);
        for (uint8_t i=0;i<jpg.frame_info.num_channels;i++)
        {
            printf("Channel #%u: id=%u, sampling=%u*%u, uses quantization table %u\n",i, \
                   jpg.frame_info.channel_info[i].id, \
                   jpg.frame_info.channel_info[i].sampling_factor>>4, \
                   jpg.frame_info.channel_info[i].sampling_factor&0xF, \
                   jpg.frame_info.channel_info[i].quant_tbl_id);
        }
        return true;
    }
}

bool read_sos(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    static const uint8_t reserved[3]={0,0x3F,0};
    if (len!=sizeof(SOS) || 1!=fread(&jpg.scan_info,sizeof(SOS),1,strm) || memcmp(jpg.scan_info.reserved,reserved,3))
    {
        puts("[X] SOF0 is corrupted.");
        return false;
    }else if (jpg.scan_info.num_channels!=3)
    {
        puts("[X] Unsupported Sampling");
        return false;
    }else
    {
        for (uint8_t i=0;i<jpg.scan_info.num_channels;i++)
        {
            printf("Channel #%u: id=%u, uses huffman table AC%u & DC%u\n",i, \
                   jpg.scan_info.channel_data[i].id, \
                   jpg.scan_info.channel_data[i].huff_tbl_id>>4, \
                   jpg.scan_info.channel_data[i].huff_tbl_id&0xF);
        }
        return true;
    }
}

bool read_dri(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    if (len!=sizeof(DRI) || 1!=fread(&jpg.dri_info,sizeof(DRI),1,strm))
    {
        puts("[X] DRI is corrupted.");
        return false;
    }
    // fix endianess
    jpg.dri_info.restart_interval=bswap16(jpg.dri_info.restart_interval);

    printf("DRI Interval is %d\n",jpg.dri_info.restart_interval);
    return true;
}

bool read_dht(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    uint8_t byte;
    while (len>0)
    {
        fread(&byte,sizeof(byte),1,strm);
        const uint8_t type=byte>>4; // 0:DC 1:AC
        const uint8_t id=byte&0x1F; // combine type and id
        if (type!=1 && type!=0)
        {
            printf("[X] Invalid Type for Huffman Table #%u.\n",id);
            return false;
        }
        if (jpg.huffman_table[id]!=NULL)
        {
            printf("[X] Huffman table #%u is already defined.\n",id);
            return false;
        }
        // allocate memory for huffman table
        jpg.huffman_table[id]=new HUFFMAN_TABLE;
        HUFFMAN_TABLE *tbl=jpg.huffman_table[id];
        tbl->num_codeword=0; // initialization

        uint8_t countByLength[16];
        if (1!=fread(&countByLength,sizeof(countByLength),1,strm))
        {
datacorrupted:
            printf("[X] Data of Huffman Table #%u is corrupted.\n",id);
            return false;
        }
        // assert(countByLength[0]==0); // true in most cases
        printf("[ ] Huffman Table #%u Data:",id);
        for (int i=1;i<=16;i++)
        {
            vbprintf(" %02x",countByLength[i-1]);
            assert(countByLength[i-1]<=(1<<i));
            tbl->num_codeword+=countByLength[i-1];
        }
        puts("");
        if (tbl->num_codeword>256)
        {
invalidtree:
            printf("[X] Huffman Table #%u is invalid.\n",id);
            return false;
        }
        else if (tbl->num_codeword>0)
        {
            // read weights
            if (1!=fread(&tbl->value,tbl->num_codeword,1,strm))
                goto datacorrupted;

            char curCodeWord[20]={'0',0};
            int curCodeWordLen=1;
            // the first codeword must be zeroes
            while (countByLength[curCodeWordLen-1]==0)
                curCodeWord[curCodeWordLen++]='0';
            --countByLength[curCodeWordLen-1];

            tbl->codeword[0]=strcpy(new char[20],curCodeWord);
            vbprintf("Codeword %s Value %d\n",tbl->codeword[0],tbl->value[0]);
            // generate other codewords
            for (int n=1;n<tbl->num_codeword;n++)
            {
                // next codeword (incremented by 1)
                int i;
                for (i=curCodeWordLen-1;i>=0;i--)
                {
                    if (++curCodeWord[i]=='1')
                        break;
                    else
                        curCodeWord[i]='0';
                }
                if (i<0) goto invalidtree;
                if (!countByLength[curCodeWordLen-1])
                {
                    // inc length (padding with 0)
                    do
                    {
                        curCodeWord[curCodeWordLen]='0';
                        curCodeWord[curCodeWordLen+1]=0;
                    }while (++curCodeWordLen<=16 && countByLength[curCodeWordLen-1]==0);
                    if (curCodeWordLen>16) goto invalidtree;
                }
                --countByLength[curCodeWordLen-1];

                tbl->codeword[n]=strcpy(new char[20],curCodeWord);
                vbprintf("Codeword %s Value %d\n",tbl->codeword[n],tbl->value[n]);
            }
            assert(0==countByLength[curCodeWordLen-1]);
        }
        if (len>=(size_t)16+tbl->num_codeword+1)
            len-=(size_t)16+tbl->num_codeword+1;
        else
        {
            puts("[X] DQT is corrupted.");
            return false;
        }
        printf("[ ] ^^^ Huffman Table #%u (Type:%s)\n",id,type?"AC":"DC");
    }
    return true;
}

bool load_jpg(const char *filePath)
{
    FILE * const fp=fopen(filePath,"rb");
    if (fp==NULL)
    {
        printf("Couldn't open file.\n");
        return false;
    }
    clock_t timestamp=clock();
    // parse data
    JPG_DATA jpg;
    memset(&jpg,0,sizeof(jpg));
    uint8_t tag[2];
    uint16_t len;
    bool foundAPP0=false;
    // read SOI
    if (!read_soi(jpg,fp))
    {
        puts("[X] read_soi() failed");
        goto error;
    }
    // read APP? tags
    tag[1]=0;
    while (1==fread(tag,sizeof(tag),1,fp) && tag[1]>=0xE0 && tag[1]<=0xEF)
    {
        #ifdef PROCESS_APPN_HEADER
        if (tag[1]==0xE0)
        {
            // APP0
            if (foundAPP0)
            {
                puts("[!] multiple app0 found");
            }
            if (!read_app0(jpg,fp))
            {
                puts("[X] read_app0() failed");
                goto error;
            }
            foundAPP0=true;
            printf("JPEG Version: %04x\n",bswap16(jpg.app0.ver));
            printf("Thumbnail: %u * %u\n",jpg.app0.thumbnail_width,jpg.app0.thumbnail_height);
        }else
        #endif
        {
            printf("skipping APP%d\n",tag[1]-0xE0);
            uint16_t len;
            fread(&len,sizeof(len),1,fp);
            fseek(fp,bswap16(len)-sizeof(len),SEEK_CUR);
        }
        tag[1]=0;
    }
    do
    {
        /*
        const long start_pos=ftell(fp);
        */
        fread(&len,sizeof(len),1,fp);
        len=bswap16(len)-2;
        switch (tag[1])
        {
        case 0xDB: // DQT
            if (!read_dqt(jpg,fp,len))
            {
                puts("[X] read_dqt() failed");
                goto error;
            }
            break;
        case 0xC0: // SOF0 (Baseline)
            if (!read_sof(jpg,fp,len))
            {
                puts("[X] read_sof() failed");
                goto error;
            }
            break;
        case 0xC1:
        case 0xC2: // Progressive
        case 0xC3: // Lossless
            puts("[X] Only Baseline Profile is Supported.");
            goto error;
            break;
        case 0xC4: // DHT
            if (!read_dht(jpg,fp,len))
            {
                puts("[X] read_dht() failed");
                goto error;
            }
            break;
        case 0xDA: // SOS
            if (!read_sos(jpg,fp,len))
            {
                puts("[X] read_sos() failed");
                goto error;
            }
            if (!is_supported_file(jpg))
            {
                puts("[X] this file is not supported");
                goto error;
            }else
            {
                puts("[ ] file format supported. ready to decode.");
            }
            printf("Time elapsed for parsing basic info: %ld\n",clock()-timestamp);

            timestamp=clock();
            if (!decode_init(jpg))
            {
                puts("[X] decoder initialization failed");
                goto error;
            }
            printf("Time elapsed for initialization: %ld\n",clock()-timestamp);

            timestamp=clock();
            if (!decode_huffman_data(jpg,fp))
            {
                puts("[X] decode_huffman_data() failed");
                goto error;
            }
            printf("Time elapsed for huffman decoding: %ld\n",clock()-timestamp);

            timestamp=clock();
            if (!decode_mcu_data(jpg,fp))
            {
                puts("[X] decode_mcu_data() failed");
                goto error;
            }
            printf("Time elapsed for IDCT and color space conversion: %ld\n",clock()-timestamp);

            puts("[ ] decoding completed.");
            break;
        case 0xDD: // DRI
            if (!read_dri(jpg,fp,len))
            {
                puts("[X] read_dri() failed");
                goto error;
            }
            break;
        case 0xD9: // EOI
            puts("[-] End of Image.");
        default:
            tag[1]=0;
            break;
        }
    }while (tag[1]!=0 && 1==fread(tag,sizeof(tag),1,fp));

error:
    fclose(fp);
    return true;
}
