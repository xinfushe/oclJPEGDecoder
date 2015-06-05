#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "huffman.h"

template <int a, class T> long HuffmanTreeNode<a,T>::NodeCount = 0;

template <int a, class T>
void
HuffmanTree<a,T>::addCode(const char * hcode, const size_t hlen, const T& hval)
{
    assert(strlen(hcode)==hlen);

}

template <int a, class T>
const HuffmanTreeNode<a,T>*
HuffmanTree<a,T>::findCode(void * const bitstream) const
{
    return NULL;
}

bool test_build_huffman_tree(const char ** code_word, const size_t num)
{
    const HuffmanTree<4,int> tree;
    return true;
}
