// C Headers
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <assert.h>

// C++ Headers
#include <utility>
using namespace std;

// jpeg structure definition
#include "jpeg.h"

uint16_t inline bswap(const uint16_t x)
{
    return (x>>8)|(x<<8);
}

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
    uint8_t tag[2];
    if (1!=fread(tag,sizeof(tag),1,strm) || tag[0]!=0xFF || tag[1]!=0xE0 || 1!=fread(&jpg.app0,sizeof(APP0),1,strm))
    {
        puts("[X] APP0 is missing.");
        return false;
    }
    // calculate the size of thumbnail image
    const size_t tn_size=(uint16_t)jpg.app0.thumbnail_width*(uint16_t)jpg.app0.thumbnail_height;
    // validate
    if (memcmp(jpg.app0.id,"JFIF\0",5) || bswap(jpg.app0.len)!=sizeof(APP0)+tn_size)
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
    uint16_t word;
    uint8_t byte;
    while (len>0)
    {
        fread(&byte,sizeof(byte),1,strm);
        const uint8_t prec=byte>>4;
        const uint8_t id=byte&0xF;
        if (id>3 || jpg.quantization_table[id]!=nullptr)
        {
            printf("[X] Quantization table #%u is invalid or already defined.\n",id);
            return false;
        }
        if (prec!=1 && prec!=0)
        {
            printf("[X] Invalid Precision Value for Quantization Table #%u.\n",id);
            return false;
        }
        switch (prec)
        {
        case 0: // 8-bit
            jpg.quantization_table[id]=new uint8_t[64];
            if (1!=fread(jpg.quantization_table[id],sizeof(uint8_t[64]),1,strm))
            {
                printf("[X] Quantization Table #%u is corrupted.\n",id);
                return false;
            }
            break;
        case 1: // 16-bit
            jpg.quantization_table[id]=new uint16_t[64];
            for (size_t i=0;i<64;i++)
            {
                if (1!=fread(&word,sizeof(uint16_t),1,strm))
                {
                    printf("[X] Quantization Table #%u is corrupted.\n",id);
                    return false;
                }else
                {
                    ((uint16_t*)jpg.quantization_table[id])[i]=bswap(word);
                }
            }
            break;
        }
        if (len>=64*(size_t)(prec+1)+1)
            len-=64*(size_t)(prec+1)+1;
        else
        {
            puts("[X] DQT is corrupted.");
            return false;
        }
        printf("[ ] Got Quantization Table #%u\n",id);
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
        puts("[X] Unsupported Encoding");
        return false;
    }else
    {
        printf("Dimensions: %u px * %u px\n",jpg.frame_info.img_width,jpg.frame_info.img_height);
        printf("Bit Depth: %u\n",jpg.frame_info.bit_depth);
        return true;

    }
}
bool read_dht(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    uint16_t word;
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
        if (jpg.huffman_table[id]!=nullptr)
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
        if (countByLength[0]>0)
        {
            // unusal
invalidtree:
            printf("[X] Huffman Table #%u is invalid.\n",id);
            return false;
        }
        printf("[ ] Huffman Table #%u Data:",id);
        for (int i=1;i<=16;i++)
        {
            printf(" %02x",countByLength[i-1]);
            assert(countByLength[i-1]<=(1<<i));
            tbl->num_codeword+=countByLength[i-1];
        }
        puts("");
        if (tbl->num_codeword>256)
            goto invalidtree;
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

            printf("Codeword %s Value %d\n",curCodeWord,tbl->value[0]);
            // generate other codewords
            for (int n=1;n<tbl->num_codeword;n++)
            {
                // inc codeword
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
                    // inc length
                    do
                    {
                        curCodeWord[curCodeWordLen]='0';
                        curCodeWord[curCodeWordLen+1]=0;
                    }while (++curCodeWordLen<=16 && countByLength[curCodeWordLen-1]==0);
                    if (curCodeWordLen>=16) goto invalidtree;
                }
                --countByLength[curCodeWordLen-1];
                strcpy(tbl->codeword[n],curCodeWord);
                printf("Codeword %s Value %d\n",curCodeWord,tbl->value[n]);
            }
            if (countByLength[curCodeWordLen-1])
            {
                goto invalidtree;
            }
        }
        if (len>=16+tbl->num_codeword+1)
            len-=16+tbl->num_codeword+1;
        else
        {
            puts("[X] DQT is corrupted.");
            return false;
        }
        printf("[ ] Got Huffman Table #%u (Type:%s)\n",id,type?"AC":"DC");
    }
    return true;
}

bool processJpgFile(const char *filePath)
{
    FILE * const fp=fopen(filePath,"rb");
    if (fp==nullptr)
    {
        printf("Couldn't open file. Error code %d.\n",errno);
        return false;
    }
    // parse data
    JPG_DATA jpg;
    memset(&jpg,0,sizeof(jpg));
    uint8_t tag[2];
    uint16_t len;
    // read SOI
    if (!read_soi(jpg,fp))
    {
        puts("[X] read_soi() failed");
        goto error;
    }
    // read APP0
    if (!read_app0(jpg,fp))
    {
        puts("[X] read_app0() failed");
        goto error;
    }
    printf("JPEG Version: %04x\n",bswap(jpg.app0.ver));
    printf("Resolution: %u * %u\n",bswap(jpg.app0.res_x),bswap(jpg.app0.res_y));
    printf("Thumbnail: %u * %u\n",jpg.app0.thumbnail_width,jpg.app0.thumbnail_height);
    // read other APP tags
    tag[1]=0;
    while (1==fread(tag,sizeof(tag),1,fp) && tag[1]>=0xE1 && tag[1]<=0xEF)
    {
        printf("skipping APP%d\n",tag[1]-0xE0);
        tag[1]=0;
        uint16_t len;
        fread(&len,sizeof(len),1,fp);
        fseek(fp,bswap(len)-sizeof(len),SEEK_CUR);
    }
    do
    {
        const long start_pos=ftell(fp);
        fread(&len,sizeof(len),1,fp);
        len=bswap(len)-2;
        switch (tag[1])
        {
        case 0xDB: // DQT
            if (!read_dqt(jpg,fp,len))
            {
                puts("[X] read_dqt() failed");
                goto error;
            }
            break;
        case 0xC0: // SOF
            if (!read_sof(jpg,fp,len))
            {
                puts("[X] read_sof() failed");
                goto error;
            }
            break;
        case 0xC4: // DHT
            if (!read_dht(jpg,fp,len))
            {
                puts("[X] read_dht() failed");
                goto error;
            }
            break;
        case 0xD9: // EOI
        default:
            tag[1]=0;
            break;
        }
    }while (tag[1]!=0 && 1==fread(tag,sizeof(tag),1,fp));

error:
    fclose(fp);
    return true;
}

int main(int argc, char **argv)
{
    if (argc<=1)
    {
        printf("Usage: %s file1 [file2 file3 ...]\n",argv[0]);
        return 0;
    }
    for (int i=1;i<argc;i++)
    {
        printf("Processing %s\n",argv[i]);
        processJpgFile(argv[i]);
        if (i+1<argc)
        {
            system("pause");
        }
    }
    return 0;
}
