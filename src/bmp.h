// definitions of bitmap file structures
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
