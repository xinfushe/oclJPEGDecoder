// C Headers
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "macro.h"
#include "bitstream.h"
#include "huffman.h"
#include "idct.h"

bool load_jpg(const char *filePath);

int main(int argc, char **argv)
{
    // run unit tests
    test_bitstream();
    test_huffman();
    #ifdef COMPILE_ONLY
        puts("tests done.");
        exit(0);
    #endif // COMPILE_ONLY

    // init IDCT library
    Initialize_Fast_IDCT();

    if (argc<=1)
    {
        printf("Usage: %s file1 [file2 file3 ...]\n",argv[0]);
        return 0;
    }
    for (int i=1;i<argc;i++)
    {
        printf("Processing %s\n",argv[i]);
        load_jpg(argv[i]);
        if (i+1<argc)
        {
            system("pause");
        }
    }
    return 0;
}
