#ifndef HUFFMAN_H_INCLUDED
#define HUFFMAN_H_INCLUDED

template <int ary, typename DataTp>
class HuffmanTreeNode
{
public:
    typedef HuffmanTreeNode<ary, DataTp> NodeType, SelfType;

    HuffmanTreeNode(const NodeType * parent):mID(++NodeCount)
    {
        static_assert(ary>=2 && ary<=16 && (ary&(ary-1))==0, "Template argument Ary must be power of 2.");
        this->mParent = parent;
        this->mNumChildren = 0;
        for (int i=0;i<ary;i++)
            this->mChildren[i] = NULL;
    }

    // access to children
    NodeType*& operator [](const int idx)
    {
        assert(idx>=0 && idx<ary);
        return mChildren[idx];
    }

    const NodeType& getChildren(const int idx) const
    {
        // must be a not-null child
        assert(idx>=0 && idx<mNumChildren && mChildren[idx]!=NULL);
        return *mChildren[idx];
    }

    NodeType& createSubTree(const int idx)
    {
        assert(idx>=mNumChildren && idx<ary && mChildren[idx]==NULL);
        return mChildren[idx]=new NodeType(this);
    }

    NodeType& createLeaf(const int idx)
    {
        assert(idx>=mNumChildren && idx<ary && mChildren[idx]==NULL);
        return mChildren[idx]=new NodeType(this);
    }

    bool isLeaf() const
    {
        return mNumChildren==0;
    }

    bool isRoot() const
    {
        return mParent==NULL;
    }

    // access to data
    const DataTp& getData() const
    {
        assert(mNumChildren==0);
        return mData;
    }

    void setData(const DataTp& data)
    {
        assert(mNumChildren==0);
        mData=data;
    }

    // advanced features
    long traverse(long lastID = -1, const bool preOrder = true) const;

private:
    NodeType *mChildren[ary];
    int mNumChildren;
    const NodeType * mParent;
    DataTp mData;

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

    const NodeType* findCode(void * const bitstream) const;

    const DataType& operator [](const char * hcode) const
    {
        // construct a temporary bitstream
    }

private:
    NodeType mRoot;

    HuffmanTree(const HuffmanTree&) = delete;
    HuffmanTree& operator = (const HuffmanTree&) = delete;
};

#endif // HUFFMAN_H_INCLUDED
