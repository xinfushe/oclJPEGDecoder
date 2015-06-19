#ifndef DECODER_H_INCLUDED
#define DECODER_H_INCLUDED

bool is_supported_file(const JPG_DATA &jpg);
bool decode_init(JPG_DATA &jpg);
bool decode_huffman_data(JPG_DATA &jpg, FILE * const strm);
bool decode_mcu_data(JPG_DATA &jpg, FILE * const strm);

#endif // DECODER_H_INCLUDED
