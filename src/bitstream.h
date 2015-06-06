#ifndef BITSTREAM_H_INCLUDED
#define BITSTREAM_H_INCLUDED

class BitStream
{
public:
    BitStream()
    {
    }

    BitStream(const uint8_t * data, const size_t len)
    {
        load(data,len);
    }

    ~BitStream()
    {
        free();
    }

    size_t getBeginPos() const
    {
        return mBytePos;
    }

    size_t getEndPos() const
    {
        return mEndPos;
    }

    uint8_t getBitPosition() const
    {
        return mBitPos;
    }

    bool isByteAligned() const
    {
        return mBitPos==0;
    }

    size_t getCapacity() const
    {
        return mCapacity;
    }

    // end-of-file indicator
    bool eof() const
    {
        return mBytePos>=mEndPos;
    }

    // test whether container is full
    bool full() const
    {
        return mEndPos==mCapacity;
    }

    // Set position of stream to the beginning
    void rewind()
    {
        mBitPos=0;
        mBytePos=0;
    }

    // clear all data
    void clear()
    {
        rewind();
        mEndPos=0;
    }

    // clear data and free allocated memory
    void free()
    {
        clear();
        if (mBitReservoir!=NULL)
        {
            delete[] mBitReservoir;
            mBitReservoir=NULL;
        }
    }

    void trim()
    {
        if (mBytePos)
        {
            if (!eof())
            {
                memcpy(mBitReservoir,mBitReservoir+mBytePos,mEndPos-mBytePos);
            }else
            {
                assert(mBitPos==0);
            }
            mEndPos-=mBytePos;
            mBytePos=0;
            // Note that mBitPos is not changed
        }
    }

    void shrinkToFit()
    {
        setCapacity(mEndPos);
    }

    void setCapacity(size_t newCap)
    {
        if (newCap==0)
            free();
        else
            reserve(newCap);
    }

    // request a change in capacity
    bool reserve(const size_t newCap)
    {
        assert(newCap>0);
        if (newCap>=mEndPos-mBytePos)
        {
            uint8_t * newMem=new uint8_t[newCap]; // realloc
            memcpy(newMem,mBitReservoir+mBytePos,mEndPos-mBytePos); // move data with trimming
            mEndPos-=mBytePos;
            mBytePos=0;
            // delete old buffer
            delete[] mBitReservoir;
            mBitReservoir=newMem;
            return true;
        }
        // it's too small to hold current data
        return false;
    }

    // skip a specified amount of bytes
    bool skipBytesAndAlign(const size_t deltaBytes)
    {
        return seekToByteAndAlign(mBytePos+deltaBytes);
    }

    // skip a specified amount of bits
    void skipBits(const uint8_t deltaBits)
    {
        mBitPos+=deltaBits;
        mBytePos+=mBitPos>>3;
        mBitPos&=7;
    }

    void alignToByte()
    {
        mBitPos=0;
    }

    void backBits(const uint8_t numBits)
    {
        mBitPos-=numBits;
        if (mBitPos&0x80) // negative
        {
            mBytePos-=(7-mBitPos)>>3;
            mBitPos&=7;
        }
    }

    bool seekToByteAndAlign(const size_t newPosition)
    {
        if (newPosition<=mEndPos)
        {
            mBytePos=newPosition;
            mBitPos=0;
            return true;
        }
        return false;
    }

    size_t append(const uint8_t * src, const size_t len);

    void load(const uint8_t * data, const size_t len)
    {
        clear();
        append(data,len);
    }

    // bit streaming
    bool frontBool() const
    {
        return mBitReservoir[mBytePos]&(1<<(7-mBitPos));
    }

    unsigned frontBit() const
    {
        return frontBool()?1:0;
    }

    uint8_t frontFullByte() const
    {
        return mBitReservoir[mBytePos];
    }

    bool nextBool()
    {
        bool ret=frontBool();
        if (++mBitPos==8)
        {
            mBitPos=0;
            mBytePos++;
        }
        return ret;
    }

    // 2 <= numBits <= 17
    uint32_t nextBits(const uint8_t numBits)
    {
        assert(numBits>=2 && numBits<=17);
        uint32_t tmp;
        if (numBits<=9)
            tmp=front9b(numBits);
        else
            tmp=front17b(numBits);
        skipBits(numBits);
        return tmp;
    }

    // numBits <= 17
    uint32_t nextBits2(const uint8_t numBits)
    {
        switch (numBits)
        {
        case 0:
            return 0;
        case 1:
            return nextBool()?1:0;
        default:
            return nextBits(numBits);
        }
    }

    uint8_t nextByte()
    {
        uint8_t ret=front9b(8);
        mBytePos++;
        return ret;
    }

    uint8_t nextFullByte()
    {
        assert(mBitPos==0);
        uint8_t ret=frontFullByte();
        mBytePos++;
        return ret;
    }

    const uint8_t * frontData() const
    {
        assert(mBitPos==0 && !eof());
        return &mBitReservoir[mBytePos];
    }

private:
    uint8_t* mBitReservoir=NULL;
    size_t mCapacity=0;
    size_t mEndPos=0;
    size_t mBytePos=0;
    uint8_t mBitPos=0;

    uint32_t front9b(const uint8_t numBits) const
    {
        uint32_t tmp=mBitReservoir[mBytePos];
        tmp=(tmp<<8)|mBitReservoir[mBytePos+1];
        tmp=tmp&((1<<(16-mBitPos))-1);
        tmp>>=16-mBitPos-numBits;
        return tmp;
    }

    uint32_t front17b(const uint8_t numBits) const
    {
        uint32_t tmp=mBitReservoir[mBytePos];
        tmp=(tmp<<8)|mBitReservoir[mBytePos+1];
        tmp=(tmp<<8)|mBitReservoir[mBytePos+2];
        tmp=tmp&((1<<(24-mBitPos))-1);
        tmp>>=24-mBitPos-numBits;
        return tmp;
    }
};

#endif // BITSTREAM_H_INCLUDED
