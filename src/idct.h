#ifndef IDCT_H_INCLUDED
#define IDCT_H_INCLUDED

void Initialize_Fast_IDCT();
void Fast_IDCT(int * block);
void idctrow(int * blk);
void idctcol(int * blk);

int Initialize_OpenCL_IDCT();

#endif // IDCT_H_INCLUDED
