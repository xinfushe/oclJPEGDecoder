# oclJPEGDecoder
GPU-accelerated JPEG Decoder based on OpenCL

Supports most JPG files. Highly optimized **Huffman decoding** algorithm running on CPU. **IDCT and color space conversion** have been offloaded to GPU, **2x-3x faster** than CPU (on Quadro K3000m and 3632qm).
