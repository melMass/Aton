/*
 Copyright (c) 2015,
 Dan Bethell, Johannes Saam, Brian Scherbinski, Vahan Sosoyan.
 All rights reserved. See Copyright.txt for more details.
 */

#include "Data.h"
#include <cstring>
#include <iostream>

using namespace aton;

Data::Data( int x, int y, 
            int width, int height, int rArea,
            int spp, long long ram, int time, const float *data ) :
    mType(-1),
    mX(x),
    mY(y),
    mWidth(width),
    mHeight(height),
    mRArea(rArea),
    mSpp(spp),
    mRam(ram),
    mTime(time)

{
    if ( data!=0 )
        mpData = const_cast<float*>(data);
}

Data::~Data()
{
}
