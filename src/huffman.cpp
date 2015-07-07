#include "stdafx.h"

#include "macro.h"
#include "huffman.h"

bool test_huffman()
{
    HuffmanTree<4,int> tree;
    HuffmanTree<8,int> tree2;
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
    tree2.addCode("00",2,567);
    tree2.addCode("010",3,123);
    assert(tree2["00"]==567);
    assert(tree2["010"]==123);
    return true;
}
