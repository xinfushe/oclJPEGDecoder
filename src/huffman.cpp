#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "huffman.h"

template <int a, class T> long HuffmanTreeNode<a,T>::NodeCount = 0;

int inline binToDec(const char * bin, const size_t len)
{
    int ret=0;
    for (size_t i=0;i<len;i++)
    {
        ret=(ret<<1)|(bin[i]=='1');
    }
    return ret;
}

template <int a, class T>
void
HuffmanTree<a,T>::addCode(const char * hcode, const size_t hlen, const T& hval)
{
    assert(strlen(hcode)==hlen);
    assert(hlen>0 && hlen<=32);

    size_t lg2;
    for (lg2=0;(1<<(uint8_t)lg2)<a;lg2++);

    HuffmanTreeNode<a,T>* node=&mRoot;
    for (size_t pos=0;pos<hlen;pos+=lg2)
    {
        if (hlen-pos==lg2) //  the last few digits
        {
            node=&(node->createLeaf(binToDec(&hcode[pos],lg2),hval));
        }
        else if (hlen-pos>lg2)
        {
            node=&(node->createSubTree(binToDec(&hcode[pos],lg2)));
        }
        else
        {
            HuffmanTreeNode<a,T>& tmpNode=*node;
            int high,low;
            high=low=binToDec(&hcode[pos],hlen-pos)<<(lg2-(hlen-pos)); // padding with 0s
            high|=(1<<(hlen-pos))-1; // padding with 1s
            assert(low<high);

            node=&(node->createLeaf(low,hval));
            for (int i=low+1;i<=high;i++)
            {
                assert(tmpNode[i]==NULL);
                tmpNode[i]=node;
            }
        }
    }
    assert(node!=NULL);
    node->setCode(hcode,hlen);
}

template <int a, class T>
const HuffmanTreeNode<a,T>*
HuffmanTree<a,T>::findCode(BitStream& strm) const
{
    size_t lg2;
    for (lg2=0;(1<<(uint8_t)lg2)<a;lg2++);

    size_t totRead=0;
    const HuffmanTreeNode<a,T> *node=&mRoot;
    while (!mRoot.isLeaf())
    {
        uint32_t buf=strm.nextBits(lg2);
        assert((buf&(~(1<<lg2)))==0); // validate the result (verbose)
        totRead+=lg2;
        node=(*node)[buf];
        if (node==NULL) return NULL; // codeword not found
    }
    assert(totRead-node->getCodeLength()<lg2);

    // return extra bits
    if (totRead>node->getCodeLength())
        strm.backBits(totRead-node->getCodeLength());
    return node;
}

bool test_build_huffman_tree()
{
    HuffmanTree<4,int> tree;
    tree.addCode("01",2,567);
    tree.addCode("100",3,123);
    tree.addCode("11111111",8,345);
    printf("%d\n",tree["100"]);
    return true;
}
