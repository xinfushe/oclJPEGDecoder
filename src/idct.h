#ifndef IDCT_H_INCLUDED
#define IDCT_H_INCLUDED

void Initialize_Fast_IDCT();
void Fast_IDCT(int * block);
void idctrow(int * blk);
void idctcol(int * blk);

int Initialize_OpenCL_IDCT();
bool clidct_create();
bool clidct_allocate_memory(const int total_blocks, const size_t image_width, const size_t image_height, const int mcu_width, const int mcu_height);
bool clidct_transfer_data_to_device(const int block_data_src[0][64], const int offset, const int count);
bool clidct_build(ColorSpace colorspace);
bool clidct_run(ColorSpace colorspace);
bool clidct_retrieve_data_from_device(int block_data_dest[0][64]);
bool clidct_retrieve_image_from_device(void *img_data_dest, const size_t img_width, const size_t img_height);
bool clidct_wait_for_completion();
bool clidct_clean_up();

#endif // IDCT_H_INCLUDED
