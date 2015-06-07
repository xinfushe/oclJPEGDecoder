// C Headers
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "bitstream.h"
#include "huffman.h"

bool loadJPG(const char *filePath);

int main(int argc, char **argv)
{
    // run unit tests
    test_bitstream();
    test_huffman();

    if (argc<=1)
    {
        printf("Usage: %s file1 [file2 file3 ...]\n",argv[0]);
        return 0;
    }
    for (int i=1;i<argc;i++)
    {
        printf("Processing %s\n",argv[i]);
        loadJPG(argv[i]);
        if (i+1<argc)
        {
            system("pause");
        }
    }
    return 0;
}
