#ifndef HUFFMAN_H_INCLUDED
#define HUFFMAN_H_INCLUDED

#include "bitstream.h"

template <int ary, typename DataTp>
class HuffmanTreeNode
{
public:
    typedef HuffmanTreeNode<ary, DataTp> NodeType, SelfType;

    HuffmanTreeNode(const NodeType * parent):mParent(parent)
    {
        mNumChildren = 0;
		for (size_t i = 0; i < ary; i++) mChildren[i] = NULL;
    }

    // initialize a leaf node
    HuffmanTreeNode(const NodeType * parent, const DataTp& data, const char * code, const size_t len):mParent(parent)
    {
        assert(parent!=NULL);
        mNumChildren = -1;
        mData = data;
        mCode = code;
        mCodeLength = len;
		for (size_t i = 0; i < ary; i++) mChildren[i] = NULL;
    }

    // access to children
    NodeType*& operator [](const int idx)
    {
        vassert(idx>=0 && idx<ary);
        return mChildren[idx];
    }

    NodeType* operator [](const int idx) const
    {
        vassert(idx>=0 && idx<ary);
        return mChildren[idx];
    }

    const NodeType& getChildren(const int idx) const
    {
        // must be a not-null child
        vassert(idx>=0 && idx<ary);
        assert(mChildren[idx]!=NULL);
        return *mChildren[idx];
    }

    NodeType& createSubTree(const int idx)
    {
        vassert(idx>=0 && idx<ary);
        assert(!isLeaf());

        if (mChildren[idx]!=NULL)
        {
            assert(!mChildren[idx]->isLeaf());
            return *mChildren[idx];
        }
        else
        {
            ++mNumChildren;
            assert(mNumChildren<=ary);
            return *(mChildren[idx]=new NodeType(this));
        }
    }

    NodeType& createLeaf(const int idx, const DataTp& data, const char * code, const size_t codelen)
    {
        vassert(idx>=0 && idx<ary);
        assert(!isLeaf() && mChildren[idx]==NULL);

        ++mNumChildren;
        assert(mNumChildren<=ary);
        return *(mChildren[idx]=new NodeType(this,data,code,codelen));
    }

    bool isLeaf() const
    {
        return mNumChildren==-1;
    }

    bool isRoot() const
    {
        return mParent==NULL;
    }

    // access to data
    operator const DataTp& () const
    {
        assert(isLeaf());
        return mData;
    }

    size_t getCodeLength() const
    {
        assert(isLeaf());
        return mCodeLength;
    }

    // advanced features
    long traverse(long lastID = -1, const bool preOrder = true) const;

private:
    NodeType *mChildren[ary];
    int mNumChildren;
    const NodeType * const mParent;
    DataTp mData;

    const char* mCode;
    size_t mCodeLength;
};

template <int ary, typename DataType>
class HuffmanTree
{
public:
    typedef HuffmanTreeNode<ary, DataType> NodeType;

    enum
    {
        // Template argument Ary must be power of 2.
        __ARY_CHECK=STATIC_ASSERT(ary>=2 && ary<=256 && (ary&(ary-1))==0)
    };

    HuffmanTree():mRoot(NULL)
    {
        calc_lg2();
    }

    // import huffman table
    HuffmanTree(const char* const* code, const DataType* val, const size_t num):mRoot(NULL)
    {
        calc_lg2();
        for (size_t i=0;i<num;i++)
        {
            addCode(code[i],strlen(code[i]),val[i]);
        }
    }

    ~HuffmanTree()
    {
        // TODO: clean
    }

    // conversion to NodeType (the way to retrieve root node)
    operator NodeType&()
    {
        return mRoot;
    }

    void addCode(const char * hcode, const size_t hlen, const DataType& hval);

    const NodeType* findCode(BitStream& strm) const;

    // cached version
    const NodeType* findCodeInCache(BitStream& strm) const;

    const DataType& operator [](const char * hcode) const
    {
        // construct a temporary bitstream
        BitStream tmp(hcode);
        return *findCode(tmp);
    }

    // advanced
    void traverse(const long firstID = 0, const bool preOrder = true) const
    {
        mRoot.traverse(firstID-1,preOrder);
    }

protected:
    NodeType mRoot;

private:
    unsigned mLg2;
    void calc_lg2()
    {
        for (mLg2=0;(1<<(uint8_t)mLg2)<ary;mLg2++);
		assert(mLg2 <= 9);
		assert(1 << mLg2 == ary);
    }

    HuffmanTree(const HuffmanTree&) = delete;
    HuffmanTree& operator = (const HuffmanTree&) = delete;
};

// though it's extremely ugly, the implementation must be put in this header file

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

    auto *node=&mRoot;
    for (size_t pos=0;pos<hlen;pos+=mLg2)
    {
        if (hlen-pos==mLg2) //  the last few digits
        {
            node=&(node->createLeaf(binToDec(&hcode[pos],mLg2),hval,hcode,hlen));
        }
        else if (hlen-pos>mLg2)
        {
            node=&(node->createSubTree(binToDec(&hcode[pos],mLg2)));
        }
        else
        {
            auto &tmpNode=*node;
            int high,low;
            high=low=binToDec(&hcode[pos],hlen-pos)<<(mLg2-(hlen-pos)); // padding with 0s
            high|=(1<<(mLg2-(hlen-pos)))-1; // padding with 1s
            vassert(low<high);

            node=&(node->createLeaf(low,hval,hcode,hlen));
            for (int i=low+1;i<=high;i++)
            {
                assert(tmpNode[i]==NULL);
                tmpNode[i]=node;
            }
        }
    }
    assert(node!=NULL);
}

template <int a, class T>
const HuffmanTreeNode<a,T>*
HuffmanTree<a,T>::findCode(BitStream& strm) const
{
	const size_t NUM_BITS_READ_ONCE = 25;
    size_t count=0, totRead=0;
	uint32_t buf;
    const auto *node=&mRoot;
	// first read
	buf = strm.front25b(NUM_BITS_READ_ONCE) << (32 - NUM_BITS_READ_ONCE);

    do
    {
		if (count + mLg2 > NUM_BITS_READ_ONCE)
		{
			// drop buffer
			strm.skipBits(count);
			totRead += count;
			count = 0;
			// refill
			buf = strm.front25b(NUM_BITS_READ_ONCE) << (32 - NUM_BITS_READ_ONCE);
		}
		// take mLg2 bits from the buffer
		const uint32_t child = buf >> (32 - mLg2);
		count += mLg2;
		buf <<= mLg2;
		// goto children
        node=(*node)[child];
		if (node == NULL)
		{
			strm.skipBits(count);
			return NULL; // codeword not found
		}
    }while (!node->isLeaf());

	count -= totRead + count - node->getCodeLength();
	strm.skipBits(count);
    return node;
}

template <int a, class T>
const HuffmanTreeNode<a,T>*
HuffmanTree<a,T>::findCodeInCache(BitStream& strm) const
{
	const size_t NUM_BITS_READ_ONCE = 16;
    size_t count=0, totRead=0;
	uint32_t buf;
    const auto *node=&mRoot;
	// first read
	buf = strm.cachedFrontBits(NUM_BITS_READ_ONCE) << (32 - NUM_BITS_READ_ONCE);
    do
    {
		if (count + mLg2 > NUM_BITS_READ_ONCE)
		{
			// drop buffer
			strm.cachedSkipBits(count);
			totRead += count;
			count = 0;
			// refill
			buf = strm.cachedFrontBits(NUM_BITS_READ_ONCE) << (32 - NUM_BITS_READ_ONCE);
		}
		// take mLg2 bits from the buffer
		const uint32_t child = buf >> (32 - mLg2);
		count += mLg2;
		buf <<= mLg2;
		// goto children
        node=(*node)[child];
		if (node == NULL)
		{
			strm.cachedSkipBits(count);
			return NULL; // codeword not found
		}
    }while (!node->isLeaf());
    // return extra bits
	count -= totRead + count - node->getCodeLength();
	strm.cachedSkipBits(count);
    return node;
}

// unit tests
bool test_huffman();

#endif // HUFFMAN_H_INCLUDED
