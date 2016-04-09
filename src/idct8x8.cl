#define W1 2841
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

#define x0 x.s0
#define x1 x.s1
#define x2 x.s2
#define x3 x.s3
#define x4 x.s4
#define x5 x.s5
#define x6 x.s6
#define x7 x.s7

kernel void _idctrow(global int * blk, const int offset)
{
    private int8 x; // x0 ~ x7
    private int8 y; // output
    private int  x8;// x8

    // load
    x.s01234567 = vload8(offset, blk).s04621753;
    x.s01 <<= 11;
    x.s0 += 128;
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
    y.s0 = (x7+x1);
    y.s1 = (x3+x2);
    y.s2 = (x0+x4);
    y.s3 = (x8+x6);
    y.s4 = (x8-x6);
    y.s5 = (x0-x4);
    y.s6 = (x3-x2);
    y.s7 = (x7-x1);
    y.s01234567 >>= 8;
    // store
    vstore8(y,offset,blk);
}

kernel void _idctcol(global int * blk)
{
    private int8 x; // x0 ~ x7
    private int8 y; // output
    private int  x8;// x8

    //intcut
    x0 = (blk[8*0]<<8) + 8192;
    x1 = (blk[8*4]<<8);
    x2 = blk[8*6];
    x3 = blk[8*2];
    x4 = blk[8*1];
    x5 = blk[8*7];
    x6 = blk[8*5];
    x7 = blk[8*3];

    //first stage
    x8 = W7*(x4+x5) + 4;
    x4 = (x8+(W1-W7)*x4);
    x5 = (x8-(W1+W7)*x5);
    x8 = W3*(x6+x7) + 4;
    x6 = (x8-(W3-W5)*x6);
    x7 = (x8-(W3+W5)*x7);
    x.s4567 >>= 3;
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
    y.s0 = (x7+x1);
    y.s1 = (x3+x2);
    y.s2 = (x0+x4);
    y.s3 = (x8+x6);
    y.s4 = (x8-x6);
    y.s5 = (x0-x4);
    y.s6 = (x3-x2);
    y.s7 = (x7-x1);
    y >>= 14;
    y = clamp(y,-256,256);
    blk[8*0] = y.s0;
    blk[8*1] = y.s1;
    blk[8*2] = y.s2;
    blk[8*3] = y.s3;
    blk[8*4] = y.s4;
    blk[8*5] = y.s5;
    blk[8*6] = y.s6;
    blk[8*7] = y.s7;
}

#undef x0
#undef x1
#undef x2
#undef x3
#undef x4
#undef x5
#undef x6
#undef x7

kernel void _idct8x8(global int * cur_block)
{
    _idctrow(cur_block,0);
    _idctrow(cur_block,1);
    _idctrow(cur_block,2);
    _idctrow(cur_block,3);
    _idctrow(cur_block,4);
    _idctrow(cur_block,5);
    _idctrow(cur_block,6);
    _idctrow(cur_block,7);

    _idctcol(cur_block+0);
    _idctcol(cur_block+1);
    _idctcol(cur_block+2);
    _idctcol(cur_block+3);
    _idctcol(cur_block+4);
    _idctcol(cur_block+5);
    _idctcol(cur_block+6);
    _idctcol(cur_block+7);
}

kernel void batch_idct(global int * block, const int num_blocks)
{
    // local int loc_block[64] __attribute ((aligned (32)));
    // we can't store block in local memory for local memory is limited.
    for (int i=get_global_id(0);i<num_blocks;i+=get_global_size(0))
    {
        global int * cur_block=block+(i<<6);
        _idct8x8(cur_block);
    }
}

kernel void batch_idct_csc_444(global int * block, const int num_blocks, write_only image2d_t image, const int num_hor_mcu)
{
    const int num_mcus=num_blocks/3;
    for (int idx_mcu=get_global_id(0);idx_mcu<num_mcus;idx_mcu+=get_global_size(0))
    {
        global int* cur_block=block+((idx_mcu*3)<<6);
        _idct8x8(cur_block);
        _idct8x8(cur_block+64);
        _idct8x8(cur_block+128);

        int2 offset=(int2)((idx_mcu%num_hor_mcu)<<3,(idx_mcu/num_hor_mcu)<<3); // (x,y)
        for (int k=0;k<64;k++)
        {
            int2 pos=(int2)(k&7,k>>3);
            int Y=*(cur_block+0+k);
            int U=*(cur_block+64+k);
            int V=*(cur_block+128+k);
            int4 rgba=(int4)(Y+1.402*V+128,Y-0.34414*U-0.71414*V+128,Y+1.772*U+128,0);
            rgba=clamp(rgba,0,255);

            write_imageui(image,offset+pos,convert_uint4(rgba));
        }

    }
}

kernel void batch_idct_csc_411(global int * block, const int num_blocks, write_only image2d_t image, const int num_hor_mcu)
{
    const int num_mcus=num_blocks/6;
    for (int idx_mcu=get_global_id(0);idx_mcu<num_mcus;idx_mcu+=get_global_size(0))
    {
        global int* cur_block=block+((idx_mcu*6)<<6);
        _idct8x8(cur_block);
        _idct8x8(cur_block+64);
        _idct8x8(cur_block+128);
        _idct8x8(cur_block+192);
        _idct8x8(cur_block+256);
        _idct8x8(cur_block+320);

		// assuming the mcu size is 16*16
        int2 offset=(int2)((idx_mcu%num_hor_mcu)<<4,(idx_mcu/num_hor_mcu)<<4); // (x,y)
		for (int y=0;y<16;y++)
		{
			for (int x=0;x<16;x++)
			{
				int Y=*(cur_block+64*(((y>>3)<<1)+(x>>3))+((y&7)<<3)+(x&7));
				int U=*(cur_block+64*4+((y>>1)<<3)+(x>>1));
				int V=*(cur_block+64*5+((y>>1)<<3)+(x>>1));
				int4 rgba=(int4)(Y+1.402*V+128,Y-0.34414*U-0.71414*V+128,Y+1.772*U+128,0);
				rgba=clamp(rgba,0,255);
				write_imageui(image,offset+(int2)(x,y),convert_uint4(rgba));
			}
		}
    }
}
