#define W1 2841
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

__kernel void _idctrow(__global int * blk)
{
    __private int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    //intcut
    if (!((x1 = blk[4]<<11) | (x2 = blk[6]) | (x3 = blk[2]) |
            (x4 = blk[1]) | (x5 = blk[7]) | (x6 = blk[5]) | (x7 = blk[3])))
    {
        blk[0]=blk[1]=blk[2]=blk[3]=blk[4]=blk[5]=blk[6]=blk[7]=blk[0]<<3;
        return;
    }
    x0 = (blk[0]<<11) + 128; // for proper rounding in the fourth stage
    //first stage
    x8 = W7*(x4+x5);
    x4 = x8 + (W1-W7)*x4;
    x5 = x8 - (W1+W7)*x5;
    x8 = W3*(x6+x7);
    x6 = x8 - (W3-W5)*x6;
    x7 = x8 - (W3+W5)*x7;
    //second stage
    x8 = x0 + x1;
    x0 -= x1;
    x1 = W6*(x3+x2);
    x2 = x1 - (W2+W6)*x2;
    x3 = x1 + (W2-W6)*x3;
    x1 = x4 + x6;
    x4 -= x6;
    x6 = x5 + x7;
    x5 -= x7;
    //third stage
    x7 = x8 + x3;
    x8 -= x3;
    x3 = x0 + x2;
    x0 -= x2;
    x2 = (181*(x4+x5)+128)>>8;
    x4 = (181*(x4-x5)+128)>>8;
    //fourth stage
    blk[0] = (x7+x1)>>8;
    blk[1] = (x3+x2)>>8;
    blk[2] = (x0+x4)>>8;
    blk[3] = (x8+x6)>>8;
    blk[4] = (x8-x6)>>8;
    blk[5] = (x0-x4)>>8;
    blk[6] = (x3-x2)>>8;
    blk[7] = (x7-x1)>>8;
}

__kernel void _idctcol(__global int * blk)
{
    __private int x0, x1, x2, x3, x4, x5, x6, x7, x8;
    //intcut
    if (!((x1 = (blk[8*4]<<8)) | (x2 = blk[8*6]) | (x3 = blk[8*2]) |
            (x4 = blk[8*1]) | (x5 = blk[8*7]) | (x6 = blk[8*5]) | (x7 = blk[8*3])))
    {
        blk[8*0]=blk[8*1]=blk[8*2]=blk[8*3]=blk[8*4]=blk[8*5]
                                            =blk[8*6]=blk[8*7]=clamp(((blk[8*0]+32)>>6),-256,256);
        return;
    }
    x0 = (blk[8*0]<<8) + 8192;
    //first stage
    x8 = W7*(x4+x5) + 4;
    x4 = (x8+(W1-W7)*x4)>>3;
    x5 = (x8-(W1+W7)*x5)>>3;
    x8 = W3*(x6+x7) + 4;
    x6 = (x8-(W3-W5)*x6)>>3;
    x7 = (x8-(W3+W5)*x7)>>3;
    //second stage
    x8 = x0 + x1;
    x0 -= x1;
    x1 = W6*(x3+x2) + 4;
    x2 = (x1-(W2+W6)*x2)>>3;
    x3 = (x1+(W2-W6)*x3)>>3;
    x1 = x4 + x6;
    x4 -= x6;
    x6 = x5 + x7;
    x5 -= x7;
    //third stage
    x7 = x8 + x3;
    x8 -= x3;
    x3 = x0 + x2;
    x0 -= x2;
    x2 = (181*(x4+x5)+128)>>8;
    x4 = (181*(x4-x5)+128)>>8;
    //fourth stage
    blk[8*0] = clamp(((x7+x1)>>14),-256,256);
    blk[8*1] = clamp(((x3+x2)>>14),-256,256);
    blk[8*2] = clamp(((x0+x4)>>14),-256,256);
    blk[8*3] = clamp(((x8+x6)>>14),-256,256);
    blk[8*4] = clamp(((x8-x6)>>14),-256,256);
    blk[8*5] = clamp(((x0-x4)>>14),-256,256);
    blk[8*6] = clamp(((x3-x2)>>14),-256,256);
    blk[8*7] = clamp(((x7-x1)>>14),-256,256);
}

__kernel void _run_idct(__global int * block)
{
    int i;
    // TODO: unrolling
    for (i=0; i<8; i++)
        _idctrow(block+8*i);

    for (i=0; i<8; i++)
        _idctcol(block+i);
}

__kernel void batch_idct(__global int * block, const unsigned int num_blocks)
{
    unsigned int i=get_global_id(0);
    for (;i<num_blocks;i+=get_global_size(0))
    {
        _run_idct(block+(i<<6));
    }
}
