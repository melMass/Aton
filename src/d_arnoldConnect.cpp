/*
Copyright (c) 2010, Dan Bethell, Johannes Saam.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    * Neither the name of RenderConnect nor the names of its contributors may be
    used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

AI_DRIVER_NODE_EXPORT_METHODS(NukeDriverMtd);

struct ShaderData
{
   //boost::thread  thread;
   //void* thread;
   renderconnect::Client *client;
};

node_parameters
{
   AiParameterSTR("host", "127.0.0.1");
   AiParameterINT("port", 9201);
   AiMetaDataSetStr(mds, NULL, "maya.translator", "nuke");
   AiMetaDataSetStr(mds, NULL, "maya.attr_prefix", "");
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
   int  port = AiNodeGetInt(node, "port");
   int width = display_window.maxx - display_window.minx +1;
   int height = display_window.maxy - display_window.miny +1;
   // now we can connect to the server and start rendering
   try
   {
       // create a new renderConnect object
       data->client = new renderconnect::Client( host, port );

       // make image header & send to server
       renderconnect::Data header( 0, 0, width, height, 4 );
       data->client->openImage( header );
   }
   catch (const std::exception &e)
   {
       AiMsgError("RenderConnect display driver", "%s", e.what());
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
   AiMsgDebug("[renderConnect] prepare bucket (%d, %d)", bucket_xo, bucket_yo);
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

       // create our data object
       renderconnect::Data packet(bucket_xo, bucket_yo,
                                  bucket_size_x, bucket_size_y,
                                  4, ptr);

       // send it to the server
       data->client->sendPixels(packet);
   }
}

driver_close
{
   AiMsgInfo("[renderConnect] driver close");

   ShaderData *data = (ShaderData*)AiDriverGetLocalData(node);
   data->client->closeImage();
}

node_finish
{
   AiMsgInfo("[renderConnect] driver finish");
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
         node->methods      = (AtNodeMethods*) NukeDriverMtd;
         node->output_type  = AI_TYPE_RGBA;
         node->name         = "driver_nuke";
         node->node_type    = AI_NODE_DRIVER;
         break;
      default:
      return false;
   }
   return true;
}

