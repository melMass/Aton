/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "aton_client.h"
#include <boost/lexical_cast.hpp>

using namespace boost::asio;

const int get_port()
{
    const char* def_port = getenv("ATON_PORT");
    int aton_port;
    
    if (def_port == NULL)
        aton_port = 9201;
    else
        aton_port = atoi(def_port);
    
    return aton_port;
}

const std::string get_host()
{
    const char* def_host = getenv("ATON_HOST");
    
    if (def_host == NULL)
        def_host = "127.0.0.1";

    std::string aton_host = def_host;
    return aton_host;
}

const bool host_exists(const char* host)
{
    boost::system::error_code ec;
    ip::address::from_string(host, ec);
    return !ec;
}

const unsigned int gen_unique_id()
{
    srand(static_cast<unsigned int>(time(NULL)));
    return rand() % 1000000 + 1;
}

const int pack_4_int(int a, int b, int c, int d)
{
    return a * 1000000 + b * 10000 + c * 100 + d;
}

// Data Class
DataHeader::DataHeader(const int& index,
                       const int& xres,
                       const int& yres,
                       const long long& rArea,
                       const int& version,
                       const float& currentFrame,
                       const float& cam_fov,
                       const float* cam_matrix,
                       const int* samples): mIndex(index),
                                            mXres(xres),
                                            mYres(yres),
                                            mRArea(rArea),
                                            mVersion(version),
                                            mCurrentFrame(currentFrame),
                                            mCamFov(cam_fov)
{
    if (cam_matrix != NULL)
        mCamMatrix = const_cast<float*>(cam_matrix);
    
    if (samples != NULL)
        mSamples = const_cast<int*>(samples);
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



// Client Class
Client::Client(std::string hostname, int port): mHost(hostname),
                                                mPort(port),
                                                mImageId(-1),
                                                mSocket(mIoService) {}


Client::~Client()
{
    disconnect();
}

void Client::connect(std::string hostname, int port)
{
    using boost::asio::ip::tcp;
    tcp::resolver resolver(mIoService);
    tcp::resolver::query query(hostname.c_str(), boost::lexical_cast<std::string>(port).c_str());
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end)
    {
        mSocket.close();
        mSocket.connect(*endpoint_iterator++, error);
    }
    if (error)
        throw boost::system::system_error(error);
}

void Client::disconnect()
{
    mSocket.close();
}

void Client::openImage(DataHeader& header)
{
    // Connect to port!
    connect(mHost, mPort);

    // Send image header message with image desc information
    int key = 0;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));
    
    // Read our imageid
    read(mSocket, buffer(reinterpret_cast<char*>(&mImageId), sizeof(int)));
    
    // Send our width & height
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mIndex), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mXres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mYres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mRArea), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mVersion), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mCurrentFrame), sizeof(float)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mCamFov), sizeof(float)));
    
    const int camMatrixSize = 16;
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mCamMatrix[0]), sizeof(float)*camMatrixSize));
    
    const int samplesSize = 6;
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mSamples[0]), sizeof(int)*samplesSize));

}

void Client::sendPixels(DataPixels& pixels)
{
    if (mImageId < 0)
    {
        throw std::runtime_error("Could not send data - image id is not valid!");
    }

    // Send data for image_id
    int key = 1;
    
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&mImageId), sizeof(int)));

    // Get size of aov name
    size_t aov_size = strlen(pixels.mAovName) + 1;

    // Get size of overall samples
    const int num_samples = pixels.mBucket_size_x * pixels.mBucket_size_y * pixels.mSpp;
    
    // Sending data to buffer
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mXres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mYres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_xo), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_yo), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_size_x), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mBucket_size_y), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mSpp), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mRam), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mTime), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&aov_size), sizeof(size_t)));
    write(mSocket, buffer(pixels.mAovName, aov_size));
    write(mSocket, buffer(reinterpret_cast<char*>(&pixels.mpData[0]), sizeof(float)*num_samples));
}

void Client::closeImage()
{
    // Send image complete message for image_id
    int key = 2;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));

    // Tell the server which image we're closing
    write(mSocket, buffer(reinterpret_cast<char*>(&mImageId), sizeof(int)));

    // Disconnect from port!
    disconnect();
}

void Client::quit()
{
    connect(mHost, mPort);
    int key = 9;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));
    disconnect();
}
