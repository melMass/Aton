/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "Data.h"

using namespace aton;

Data::Data(const int& x,
           const int& y,
           const int& width,
           const int& height,
           const long long& rArea,
           const int& version,
           const float& currentFrame,
           const int& spp,
           const long long& ram,
           const int& time,
           const char* aovName, 
           const float* data ): mType(-1),
                                mX(x),
                                mY(y),
                                mWidth(width),
                                mHeight(height),
                                mRArea(rArea),
                                mVersion(version),
                                mCurrentFrame(currentFrame),
                                mSpp(spp),
                                mRam(ram),
                                mTime(time),
                                mAovName(aovName)
{
    if (data != 0)
        mpData = const_cast<float*>(data);
}

void Data::deAllocAovName()
{
    delete[] mAovName;
    mAovName = NULL;
}

Data::~Data()
{
}
