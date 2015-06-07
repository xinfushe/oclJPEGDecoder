#ifndef HUFFMAN_H_INCLUDED
#define HUFFMAN_H_INCLUDED

#include "bitstream.h"

template <int ary, typename DataTp>
class HuffmanTreeNode
{
public:
    typedef HuffmanTreeNode<ary, DataTp> NodeType, SelfType;

    HuffmanTreeNode(const NodeType * parent):mParent(parent),mID(++NodeCount)
    {
        mNumChildren = 0;
    }

    HuffmanTreeNode(const NodeType * parent, const DataTp& data):mParent(parent),mID(++NodeCount)
    {
        assert(parent!=NULL);
        mNumChildren = -1;
        mData = data;
    }

    // access to children
    NodeType*& operator [](const int idx)
    {
        assert(idx>=0 && idx<ary);
        return mChildren[idx];
    }

    NodeType* operator [](const int idx) const
    {
        assert(idx>=0 && idx<ary);
        return mChildren[idx];
    }

    const NodeType& getChildren(const int idx) const
    {
        // must be a not-null child
        assert(idx>=0 && idx<ary && mChildren[idx]!=NULL);
        return *mChildren[idx];
    }

    NodeType& createSubTree(const int idx)
    {
        assert(idx>=0 && idx<ary && mChildren[idx]==NULL);
        assert(!isLeaf());

        if (mChildren[idx]!=NULL)
            return *mChildren[idx];
        else
        {
            ++mNumChildren;
            assert(mNumChildren<=ary);
            return *(mChildren[idx]=new NodeType(this));
        }
    }

    NodeType& createLeaf(const int idx, const DataTp& data)
    {
        assert(idx>=0 && idx<ary && mChildren[idx]==NULL);
        assert(!isLeaf());

        ++mNumChildren;
        assert(mNumChildren<=ary);
        return *(mChildren[idx]=new NodeType(this, data));
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
    const DataTp& getData() const
    {
        assert(isLeaf());
        return mData;
    }

    operator const DataTp& () const
    {
        return getData();
    }

    void setData(const DataTp& data)
    {
        assert(isLeaf());
        mData=data;
    }

    size_t getCodeLength() const
    {
        assert(isLeaf());
        return mCodeLength;
    }

    void setCode(const char * code, const size_t len)
    {
        assert(isLeaf());
        mCode=code;
        mCodeLength=len;
    }

    // advanced features
    long traverse(long lastID = -1, const bool preOrder = true) const;

private:
    NodeType *mChildren[ary]={NULL};
    int mNumChildren;
    const NodeType * const mParent;
    DataTp mData;

    const char* mCode;
    size_t mCodeLength;
    const long mID;
    static long NodeCount;
};

template <int ary, typename DataType>
class HuffmanTree
{
public:
    typedef HuffmanTreeNode<ary, DataType> NodeType;

    HuffmanTree():mRoot(NULL)
    {
        static_assert(ary>=2 && ary<=16 && (ary&(ary-1))==0, "Template argument Ary must be power of 2.");
    }

    ~HuffmanTree()
    {
    }

    // conversion to NodeType (the way to retrieve root node)
    operator NodeType&()
    {
        return mRoot;
    }

    void addCode(const char * hcode, const size_t hlen, const DataType& hval);

    const NodeType* findCode(BitStream& strm) const;

    const DataType& operator [](const char * hcode) const
    {
        // construct a temporary bitstream
        BitStream tmp((uint8_t*)hcode,strlen(hcode));
        return *findCode(tmp);
    }

    // advanced
    void traverse(const long firstID = 0, const bool preOrder = true) const
    {
        mRoot.traverse(firstID-1,preOrder);
    }
private:
    NodeType mRoot;

    HuffmanTree(const HuffmanTree&) = delete;
    HuffmanTree& operator = (const HuffmanTree&) = delete;
};

#endif // HUFFMAN_H_INCLUDED
