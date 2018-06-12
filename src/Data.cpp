/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "Data.h"
#include <iostream>

Data::Data(): mType(-1) {}
Data::~Data() {}


DataHeader::DataHeader(const int& xres,
                       const int& yres,
                       const long long& rArea,
                       const int& version,
                       const float& currentFrame,
                       const float& cam_fov,
                       const float* cam_matrix): mXres(xres),
                                                 mYres(yres),
                                                 mRArea(rArea),
                                                 mVersion(version),
                                                 mCurrentFrame(currentFrame),
                                                 mCamFov(cam_fov)
{
    if (cam_matrix != NULL)
        mCamMatrix = const_cast<float*>(cam_matrix);
}
DataHeader::~DataHeader() {}



DataPixels::DataPixels(const int& xres,
                       const int& yres,
                       const int& bucket_xo,
                       const int& bucket_yo,
                       const int& bucket_size_x,
                       const int& bucket_size_y,
                       const int& spp,
                       const long long& ram,
                       const int& time,
                       const char* aovName,
                       const float* data) : mXres(xres),
                                            mYres(yres),
                                            mBucket_xo(bucket_xo),
                                            mBucket_yo(bucket_yo),
                                            mBucket_size_x(bucket_size_x),
                                            mBucket_size_y(bucket_size_y),
                                            mSpp(spp),
                                            mRam(ram),
                                            mTime(time),
                                            mAovName(aovName)

{
    if (data != NULL)
        mpData = const_cast<float*>(data);
}

DataPixels::~DataPixels() {}

void DataPixels::free()
{
    delete[] mAovName;
    mAovName = NULL;
}
