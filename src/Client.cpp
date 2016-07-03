/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "Client.h"
#include <boost/lexical_cast.hpp>

using namespace aton;
using namespace boost::asio;

Client::Client(std::string hostname, int port): mHost(hostname),
                                                mPort(port),
                                                mImageId(-1),
                                                mSocket(mIoService)
{
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

Client::~Client()
{
    disconnect();
}

void Client::openImage(Data& header)
{
    // Connect to port!
    connect(mHost, mPort);

    // Send image header message with image desc information
    int key = 0;
    write(mSocket, buffer(reinterpret_cast<char*>(&key), sizeof(int)));

    // Read our imageid
    read(mSocket, buffer(reinterpret_cast<char*>(&mImageId), sizeof(int)));

    // Send our width & height
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mBucket_size_x), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mBucket_size_y), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mRArea), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mVersion), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&header.mCurrentFrame), sizeof(float)));
}

void Client::sendPixels(Data& data)
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
    size_t aov_size = strlen(data.mAovName) + 1;

    // Get size of overall samples
    int num_samples = data.mBucket_size_x * data.mBucket_size_y * data.mSpp;
    
    // Sending data to buffer
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mXres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mYres), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mBucket_xo), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mBucket_yo), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mBucket_size_x), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mBucket_size_y), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mRArea), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mVersion), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mCurrentFrame), sizeof(float)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mSpp), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mRam), sizeof(long long)));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mTime), sizeof(int)));
    write(mSocket, buffer(reinterpret_cast<char*>(&aov_size), sizeof(size_t)));
    write(mSocket, buffer(data.mAovName, aov_size));
    write(mSocket, buffer(reinterpret_cast<char*>(&data.mpData[0]), sizeof(float)*num_samples));
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
