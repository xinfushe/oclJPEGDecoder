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

struct DQT
{
    uint16_t len;

};
#pragma pack()

struct JPG_DATA
{
    APP0 app0;
    void *quantization_table[4];
    void *huffman_table[16];
    void *thumbnail;
};

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

bool read_dht(JPG_DATA &jpg, FILE * const strm, size_t len)
{
    uint16_t word;
    uint8_t byte;
    while (len>0)
    {
        fread(&byte,sizeof(byte),1,strm);
        const uint8_t type=byte>>4; // 0:DC 1:AC
        const uint8_t id=byte&0xF;
        if (jpg.huffman_table[id]!=nullptr)
        {
            printf("[X] Huffman table #%u is already defined.\n",id);
            return false;
        }
        if (type!=1 && type!=0)
        {
            printf("[X] Invalid Type for Huffman Table #%u.\n",id);
            return false;
        }
        len-=0+1;
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
        case 0xC4: // DHT
            goto error;
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
        if (i+1<argc) system("pause");
    }
    return 0;
}
