/*
 Copyright (c) 2015,
 Dan Bethell, Johannes Saam, Vahan Sosoyan.
 All rights reserved. See Copyright.txt for more details.
 */

#ifndef ATON_SERVER_H_
#define ATON_SERVER_H_

#include "Data.h"
#include <boost/asio.hpp>

//! \namespace aton
namespace aton
{
    /*! \class Server
     * \brief Represents a listening Server, ready to accept incoming images.
     *
     * This class wraps up the provision of a TCP port, and handles incoming
     * connections from Client objects when they're ready to send image data.
     */
    class Server
    {
    public:
        /*! \brief Constructor.
         *
         * Creates a new server. By default the Server is not connected at
         * creation time.
         */
        Server();
        /*! \brief Constructor.
         *
         * Creates a new server and calls connect() with the specified port
         * number.
         */
        Server( int port );
        /*! \brief Destructor.
         *
         *  Shuts down the server, closing any open ports if the server is
         *  connected.
         */
        ~Server();
        
        /*! \brief Connects the server to a port.
         *
         * If true is passed as the second parameter then the server will
         * search for the first available port if the specified one is not
         * available. To find out which port the server managed to connect to,
         * call getPort() afterwards.
         */
        void connect( int port, bool search=false );

        /*! \brief Sets up the server to accept an incoming Client connections.
         */
        void accept();

        /*! \brief Listens for incoming messages from a Client.
         *
         * This function blocks (and so may be require running on a separate
         * thread), returning once a Client has sent a message.
         *
         * The returned Data object is filled with the relevant information and
         * passed back ready for handling by the parent application.
         */
        Data listen();

        /*! \brief Sends a 'quit' message to the server.
         *
         * This can be used to exit a listening loop running on a separate
         * thread.
         */
        void quit();

        //! Returns whether or not the server is connected to a port.
        bool isConnected(){ return mAcceptor.is_open(); }

        //! Returns the port the server is currently connected to.
        int getPort(){ return mPort; }

    private:

        // the port we're listening to
        int mPort;

        // boost::asio tcp stuff
        boost::asio::io_service mIoService;
        boost::asio::ip::tcp::socket mSocket;
        boost::asio::ip::tcp::acceptor mAcceptor;
    };
}

#endif // ATON_SERVER_H_
