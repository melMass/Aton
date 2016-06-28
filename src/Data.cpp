/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "Data.h"
#include <cstring>
#include <iostream>

using namespace aton;

Data::Data(int x, 
           int y,
           int width, 
           int height, 
           long long rArea, 
           int version, 
           float currentFrame,
           int spp, 
           long long ram, 
           int time, 
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
                                mTime(time)
{
    if (aovName != NULL)
        mAovName = const_cast<char*>(aovName);

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
