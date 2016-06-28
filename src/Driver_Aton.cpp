/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include <ai.h>

#include "Data.h"
#include "Client.h"

using boost::asio::ip::tcp;

AI_DRIVER_NODE_EXPORT_METHODS(AtonDriverMtd);

struct ShaderData
{
   aton::Client* client;
};

const char* getHost()
{
    const char* aton_host = getenv("ATON_HOST");
    
    if (aton_host == NULL)
        aton_host = "127.0.0.1";
    
    char* host = new char[strlen(aton_host)+1];
    strcpy(host, aton_host);
    aton_host = host;
    
    return aton_host;
}

int getPort()
{
    const char* def_port = getenv("ATON_PORT");
    int aton_port;
    
    if (def_port == NULL)
        aton_port = 9201;
    else
        aton_port = atoi(def_port);
    
    return aton_port;
}

node_parameters
{
    AiParameterSTR("host", getHost());
    AiParameterINT("port", getPort());
    AiMetaDataSetStr(mds, NULL, "maya.translator", "aton");
    AiMetaDataSetStr(mds, NULL, "maya.attr_prefix", "");
    AiMetaDataSetBool(mds, NULL, "display_driver", true);
    AiMetaDataSetBool(mds, NULL, "single_layer_driver", false);
}

node_initialize
{
    AiDriverInitialize(node, true, AiMalloc(sizeof(ShaderData)));
}

node_update {}

driver_supports_pixel_type { return true; }

driver_extension { return NULL; }

driver_open
{
    // Construct full version number
    char arch[3], major[3], minor[3], fix[3];
    AiGetVersion(arch, major, minor, fix);
    int version = atoi(arch) +
                  atoi(major) * 100 +
                  atoi(minor) * 10000 +
                  atoi(fix) * 1000000;
    
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
    
    // Get Frame number
    AtNode* options = AiUniverseGetOptions();
    float currentFrame = AiNodeGetFlt(options, "frame");
    
    // Get Host and Port
    const char* host = AiNodeGetStr(node, "host");
    int port = AiNodeGetInt(node, "port");
    
    // Get resolution and area
    int width = display_window.maxx - display_window.minx +1;
    int height = display_window.maxy - display_window.miny +1;
    int rWidth = data_window.maxx - data_window.minx +1;
    int rHeight = data_window.maxy - data_window.miny +1;
    long long rArea = rWidth * rHeight;

    try // Now we can connect to the server and start rendering
    {
       // Create a new aton object
       data->client = new aton::Client( host, port );

       // Make image header & send to server
       aton::Data header( 0, 0, width, height, rArea, version, currentFrame);
       data->client->openImage(header);
    }
    catch (const std::exception &e)
    {
        const char *err = e.what();
        AiMsgError("Aton display driver %s", err);
    }
}

driver_needs_bucket { return true; }

driver_prepare_bucket
{
    AiMsgDebug("[Aton] prepare bucket (%d, %d)", bucket_xo, bucket_yo);
}

driver_process_bucket { }

driver_write_bucket
{
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);

    int pixel_type;
    int spp = 0;
    const void* bucket_data;
    const char* aov_name;

    while (AiOutputIteratorGetNext(iterator, &aov_name, &pixel_type, &bucket_data))
    {
        const float* ptr = reinterpret_cast<const float*> (bucket_data);
        long long ram = AiMsgUtilGetUsedMemory();
        unsigned int time = AiMsgUtilGetElapsedTime();

        switch (pixel_type)
        {
            case(AI_TYPE_FLOAT):
                spp = 1;
                break;
            case(AI_TYPE_RGBA):
                spp = 4;
                break;
            default:
                spp = 3;
        }
        
        // Create our data object
        aton::Data packet(bucket_xo, bucket_yo,
                          bucket_size_x, bucket_size_y,
                          0, 0, 0, spp, ram, time, aov_name, ptr);

        // Send it to the server
        data->client->sendPixels(packet);
    }
}

driver_close
{
    AiMsgInfo("[Aton] driver close");

    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
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
   
    // Release the driver
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
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
            node->methods = (AtNodeMethods*) AtonDriverMtd;
            node->output_type = AI_TYPE_RGBA;
            node->name = "driver_aton";
            node->node_type = AI_NODE_DRIVER;
            break;
        default:
        return false;
    }
    return true;
}
