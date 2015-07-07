#ifndef BITSTREAM_H_INCLUDED
#define BITSTREAM_H_INCLUDED

class BitStream
{
public:
    BitStream()
    {
        clear();
    }

    BitStream(const size_t cap)
    {
        clear();
        reserve(cap);
    }

    BitStream(const uint8_t * data, const size_t len)
    {
        load(data,len);
    }

    // slow implementation
    BitStream(const char * str)
    {
        vassert(str!=NULL);
        clear();
        reserve((strlen(str)+7)>>3);
        for (;*str;str++) writeBit(*str=='1');
        rewind();
    }

    ~BitStream()
    {
        free();
    }

    size_t getCurrentPos() const
    {
        return mBytePos;
    }

    size_t getEndPos() const
    {
        return mEndPos;
    }

    size_t getSize() const
    {
        return mEndPos-mBytePos;
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

    bool eof() const
    {
        return mBytePos>=mEndPos;
    }

    // test whether container is full
    bool full() const
    {
        return mEndPos==mCapacity;
    }

    void rewind()
    {
        mBitPos=0;
        mBytePos=0;
    }

    // clear all data
    void clear()
    {
        mEndPos=0;
        rewind();
    }

    // clear data and free allocated memory
    void free()
    {
        clear();
        if (mBitReservoir!=NULL)
        {
            delete[] mBitReservoir;
            mBitReservoir=NULL;
            mCapacity=0;
        }
    }

    void trim()
    {
        if (mBytePos)
        {
            if (!eof())
            {
                moveDataTo(mBitReservoir);
            }else
            {
                clear();
            }
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
        if (newCap>=fixPosition())
        {
            uint8_t * const oldMem=mBitReservoir;
            uint8_t * const newMem=new uint8_t[newCap+4]; // realloc
            moveDataTo(newMem); // move data with trimming
            mCapacity=newCap; // update capacity
            if (oldMem) delete[] oldMem; // delete old buffer
            return true;
        }
        else
        {
            // it would be too small to hold current data
            return false;
        }
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
        int nPos=mBitPos-(int)numBits;
        if (nPos<0)
        {
            mBytePos-=(7-nPos)>>3;
            mBitPos=nPos&7;
        }else
        {
            mBitPos=nPos;
        }
    }

    bool seekToByteAndAlign(const size_t newPosition)
    {
        if (newPosition<=mEndPos)
        {
            mBytePos=newPosition;
            alignToByte();
            return true;
        }
        return false;
    }

    size_t append(const uint8_t * src, const size_t len);
    size_t append(const uint8_t * begin, const uint8_t * end)
    {
        // note that the byte at END is not included
        return append(begin,end-begin);
    }

    void load(const uint8_t * data, const size_t len)
    {
        clear();
        append(data,len);
    }

    // bit streaming
    bool frontBit() const
    {
        return 0!=(mBitReservoir[mBytePos]&(1<<(7-mBitPos)));
    }

    uint8_t frontFullByte() const
    {
        return mBitReservoir[mBytePos];
    }

    // write a bit to the current position (can overwrite)
    void writeBit(bool b)
    {
        const uint8_t mask=1<<(7-mBitPos);
        if (b)
            mBitReservoir[mBytePos]|=mask;
        else
            mBitReservoir[mBytePos]&=~mask;
        moveToNextBit();
    }

    bool nextBit()
    {
        bool ret=frontBit();
        moveToNextBit();
        return ret;
    }

    void moveToNextBit()
    {
        // to avoid if branch
        skipBits(1);
    }

    // numBits <= 25
    uint32_t nextBits(const uint8_t numBits)
    {
        uint32_t tmp;
        vassert(numBits!=0 && numBits<=25);
		if (numBits == 1)
			return nextBit();
		else if (numBits <= 9)
		{
			tmp = front9b(numBits);
		}
		else if (numBits <= 17)
		{
			tmp = front17b(numBits);
		}
		else if (numBits <= 25)
		{
			tmp = front25b(numBits);
		}
        skipBits(numBits);
        return tmp;
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
        // don't call me when eof is reached pls
        vassert(!eof());
        return &mBitReservoir[mBytePos];
    }

private:
    uint8_t* mBitReservoir=NULL;
    size_t mCapacity=0;
    size_t mEndPos;
    size_t mBytePos;
    uint8_t mBitPos;

    uint32_t front9b(const uint8_t numBits) const
    {
        uint16_t tmp=bswap16(*(uint16_t*)&mBitReservoir[mBytePos]);
        tmp&=(1<<(16-mBitPos))-1;
        tmp>>=16-mBitPos-numBits;
        return tmp;
    }

    uint32_t front17b(const uint8_t numBits) const
    {
        uint32_t tmp=bswap32(*(uint32_t*)&mBitReservoir[mBytePos])>>8;
        tmp&=(1<<(24-mBitPos))-1;
        tmp>>=24-mBitPos-numBits;
        return tmp;
    }

    uint32_t front25b(const uint8_t numBits) const
    {
        uint32_t tmp=bswap32(*(uint32_t*)&mBitReservoir[mBytePos]);
        tmp&=(1<<(32-mBitPos))-1;
        tmp>>=32-mBitPos-numBits;
        return tmp;
    }

    // adjust pointer when eof is reached, and return current window size
    size_t fixPosition()
    {
        if (mBytePos>=mEndPos)
        {
            assert(mBytePos<=mCapacity);
            mBytePos=mEndPos;
            mBitPos=0;
        }
        return mEndPos-mBytePos;
    }

    void moveDataTo(uint8_t* newBuffer)
    {
        vassert(newBuffer!=NULL);
        if (fixPosition())
        {
            memcpy(newBuffer,frontData(),mEndPos-mBytePos);
        }
        mEndPos-=mBytePos;
        mBytePos=0;
        mBitReservoir=newBuffer;
        // Note that the old buffer is not freed here
        // and that mBitPos is not changed
    }
};

// unit tests
bool test_bitstream();

#endif // BITSTREAM_H_INCLUDED
