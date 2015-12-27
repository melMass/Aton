/*
 Copyright (c) 2015,
 Dan Bethell, Johannes Saam, Brian Scherbinski, Vahan Sosoyan.
 All rights reserved. See Copyright.txt for more details.
 */

#include <iostream>
#include <exception>
#include <cstring>

#include "Client.h"
#include "Data.h"


#include <ai.h>
#include <ai_critsec.h>
#include <ai_drivers.h>
#include <ai_filters.h>
#include <ai_msg.h>
#include <ai_render.h>
#include <ai_universe.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/format.hpp>

#include <stdio.h>
#include <iostream>
#include <deque>

using namespace std;
using boost::asio::ip::tcp;

AI_DRIVER_NODE_EXPORT_METHODS(AtonDriverMtd);

struct ShaderData
{
   //boost::thread  thread;
   //void* thread;
   aton::Client *client;
};

node_parameters
{
    AiParameterSTR("host", "127.0.0.1");
    AiParameterINT("port", 9201);
    AiMetaDataSetStr(mds, NULL, "maya.translator", "aton");
    AiMetaDataSetStr(mds, NULL, "maya.attr_prefix", "");
    AiMetaDataSetBool(mds, NULL, "display_driver", true);
    AiMetaDataSetBool(mds, NULL, "single_layer_driver", false);
}

node_initialize
{
   AiDriverInitialize(node, true, AiMalloc(sizeof(ShaderData)));
}

node_update
{
}

driver_supports_pixel_type
{
   return true;
}

driver_extension
{
   return NULL;
}

driver_open
{

    ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);

    const char* host = AiNodeGetStr(node, "host");
    int port = AiNodeGetInt(node, "port");
    int width = display_window.maxx - display_window.minx +1;
    int height = display_window.maxy - display_window.miny +1;

    int rWidth = data_window.maxx - data_window.minx +1;
    int rHeight = data_window.maxy - data_window.miny +1;
    
    int rArea = rWidth * rHeight;

    // now we can connect to the server and start rendering
    try
    {
       // create a new aton object
       data->client = new aton::Client( host, port );

       // make image header & send to server
       aton::Data header( 0, 0, width, height, 4, 0, 0, rArea );
       data->client->openImage( header );
    }
    catch (const std::exception &e)
    {
        const char *err = e.what();
        AiMsgError("Aton display driver", "%s", err);

    }


//   data->nchannels = 0;
//   unsigned int i = 0;
//   while (AiOutputIteratorGetNext(iterator, &aov_name, &pixel_type, NULL))
//   {
//      aov_list.push_back(aov_name);
//      if (i > 0)
//         aov_names += ",";
//      aov_names += std::string("\"") + aov_name + "\"";
//      switch(pixel_type)
//      {
//         case AI_TYPE_RGB:
//            data->nchannels = MAX(data->nchannels, 3);
//            break;
//         case AI_TYPE_RGBA:
//            data->nchannels = MAX(data->nchannels, 4);
//            break;
//         default:
//            break;
//      }
//      i++;
//   }

}

driver_needs_bucket
{
   return true;
}

driver_prepare_bucket
{
   AiMsgDebug("[Aton] prepare bucket (%d, %d)", bucket_xo, bucket_yo);
}

driver_process_bucket
{

}

driver_write_bucket
{
   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);

   int         pixel_type;
   const void* bucket_data;
   const char* aov_name;

   while (AiOutputIteratorGetNext(iterator, &aov_name, &pixel_type, &bucket_data))
   {
       const float *ptr = reinterpret_cast<const float*> (bucket_data);
       unsigned long long ram = AiMsgUtilGetUsedMemory();
       unsigned int time = AiMsgUtilGetElapsedTime();
       
       // create our data object
       aton::Data packet(bucket_xo, bucket_yo,
                                  bucket_size_x, bucket_size_y,
                                  4, ram, time, 0, ptr);

       // send it to the server
       data->client->sendPixels(packet);
   }
}

driver_close
{
   AiMsgInfo("[Aton] driver close");

   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);
   try
   {
		data->client->closeImage();
   }
   catch (const std::exception &e)
   {
	   AiMsgError("Error occured when trying to close connection");
   }
}

node_finish
{
   AiMsgInfo("[Aton] driver finish");
   // release the driver

   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);
   delete data->client;

   AiFree(data);
   AiDriverDestroy(node);
}

node_loader
{
   sprintf(node->version, AI_VERSION);

   switch (i)
   {
      case 0:
         node->methods      = (AtNodeMethods*) AtonDriverMtd;
         node->output_type  = AI_TYPE_RGBA;
         node->name         = "driver_aton";
         node->node_type    = AI_NODE_DRIVER;
         break;
      default:
      return false;
   }
   return true;
}

