#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "macro.h"
#include "bitstream.h"

size_t BitStream::append(const uint8_t * src, const size_t len)
{
    assert(src!=NULL && len>0);
    const size_t minSize=fixPosition()+len;
    if (minSize>mCapacity) // needs extra space
    {
        if (!reserve(minSize))
            return 0; // unexpected error
    }
    if (mEndPos+len>mCapacity)
    {
        trim();
    }
    assert(mEndPos+len<=mCapacity);
    memcpy(&mBitReservoir[mEndPos],src,len);
    mEndPos+=len;
    return len;
}

bool test_bitstream()
{
    return true;
}
