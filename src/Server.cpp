/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "Server.h"
#include "Client.h"
#include <boost/lexical_cast.hpp>

using namespace aton;
using namespace boost::asio;

Server::Server(): mPort(0),
                  mSocket(mIoService),
                  mAcceptor(mIoService)
{
}

Server::Server(int port): mPort(0),
                          mSocket(mIoService),
                          mAcceptor(mIoService)
{
    connect(port);
}

Server::~Server()
{
    if (mAcceptor.is_open())
        mAcceptor.close();
}

void Server::connect(int port, bool search)
{
    // Disconnect if necessary
    if (mAcceptor.is_open())
        mAcceptor.close();

    // Reconnect at specified port
    int start_port = port;
    while (!mAcceptor.is_open() && port < start_port + 99)
    {
        try
        {
            using boost::asio::ip::tcp;
            tcp::endpoint endpoint(ip::tcp::v4(), port);
            mAcceptor.open(endpoint.protocol());
            mAcceptor.set_option(ip::tcp::acceptor::reuse_address(false));
            mAcceptor.bind(endpoint);
            mAcceptor.listen();
            mPort = port;
        }
        catch (...)
        {
            mAcceptor.close();
            if (!search)
                break;
            else
                port++;
        }
    }

    // Handle failed connection
    if (!mAcceptor.is_open())
    {
        char buffer[32];
        sprintf(buffer, "port: %d", start_port);
        if (search)
            sprintf(buffer, "port: %d-%d", start_port, start_port + 99);
        std::string error = "Failed to connect to port ";
        error += buffer;
        throw std::runtime_error( error.c_str() );
    }
}

void Server::quit()
{
    std::string hostname("localhost");
    aton::Client client(hostname, mPort);
    client.quit();
}

void Server::accept()
{
    if (mSocket.is_open())
        mSocket.close();
    mAcceptor.accept(mSocket);
}

Data Server::listen()
{
    Data d;

    // Read the key from the incoming data
    try
    {
        read(mSocket, buffer(reinterpret_cast<char*>(&d.mType), sizeof(int)));

        switch(d.mType)
        {
            case 0: // Open image
            {
                // Send back an image id
                int image_id = 1;
                write(mSocket, buffer(reinterpret_cast<char*>(&image_id), sizeof(int)));
                
                // Read data from the buffer
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mXres), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mYres), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mRArea), sizeof(long long)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mVersion), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mCurrentFrame), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mCamFov), sizeof(float)));
                
                int camMatrixSize = 16;
                d.mCamMatrixStore.resize(camMatrixSize);
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mCamMatrixStore[0]), sizeof(float)*camMatrixSize));
                break;
            }
            case 1: // Image data
            {
                // Receive image id
                int image_id;
                read(mSocket, buffer(reinterpret_cast<char*>(&image_id), sizeof(int)) );

                // Read data from the buffer
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mXres), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mYres), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mBucket_xo), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mBucket_yo), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mBucket_size_x), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mBucket_size_y), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mRArea), sizeof(long long)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mVersion), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mCurrentFrame), sizeof(float)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mSpp), sizeof(int)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mRam), sizeof(long long)));
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mTime), sizeof(int)));

                // Get aov name's size
                size_t aov_size;
                read(mSocket, buffer(reinterpret_cast<char*>(&aov_size), sizeof(size_t)));

                // Get aov name
                char* aov_name = new char[aov_size];
                read(mSocket, buffer(aov_name, aov_size));
                d.mAovName = aov_name;

                // Get pixels
                int num_samples = d.bucket_size_x() * d.bucket_size_y() * d.spp();
                d.mPixelStore.resize(num_samples);
                read(mSocket, buffer(reinterpret_cast<char*>(&d.mPixelStore[0]), sizeof(float)*num_samples));
                break;
            }
            case 2: // Close image
            {
                int image_id;
                read(mSocket, buffer(reinterpret_cast<char*>(&image_id), sizeof(int)));
                mSocket.close();
                break;
            }
            case 9: // quit
            {
                mSocket.close();
                
                // This fixes all nuke destructor issues on windows
                mAcceptor.close();
                break;
            }
        }
    }
    catch( ... )
    {
        mSocket.close();
        throw std::runtime_error("Could not read from socket!");
    }
    return d;
}
