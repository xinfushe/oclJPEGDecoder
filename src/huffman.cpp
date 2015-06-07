#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "macro.h"
#include "huffman.h"

bool test_huffman()
{
    HuffmanTree<4,int> tree;
    tree.addCode("01",2,567);
    tree.addCode("100",3,123);
    tree.addCode("11111111",8,345);
    assert(tree["100"]==123);
    assert(tree["1000"]==123);
    assert(tree["1001"]==123);
    assert(tree["11111111"]==345);
    assert(tree["01"]==567);
    assert(tree["010"]==567);
    assert(tree["011"]==567);
    assert(tree["0111"]==567);
    return true;
}
