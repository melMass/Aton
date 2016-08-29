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
    aton::Client* client, *extra_client;
    bool extraHost;
    int xres, yres, min_x, min_y, max_x, max_y;
};

const char* getHost()
{
    const char* aton_host = getenv("ATON_HOST");
    
    if (aton_host == NULL)
        aton_host = "127.0.0.1";

    char* host = new char[strlen(aton_host) + 1];
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
    AiParameterSTR("extra_host", "");
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
    
    int version = atoi(arch) * 1000000 +
                  atoi(major) * 10000 +
                  atoi(minor) * 100 +
                  atoi(fix);
    
    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
    
    // Get Frame number
    AtNode* options = AiUniverseGetOptions();
    float currentFrame = AiNodeGetFlt(options, "frame");
    
    // Get Host and Port
    const char* host = AiNodeGetStr(node, "host");
    const char* extra_host = AiNodeGetStr(node, "extra_host");
    
    int port = AiNodeGetInt(node, "port");
    
    // Get Resolution
    int xres = AiNodeGetInt(options, "xres");
    int yres = AiNodeGetInt(options, "yres");
    
    // Get Regions
    int min_x = AiNodeGetInt(options, "region_min_x");
    int min_y = AiNodeGetInt(options, "region_min_y");
    int max_x = AiNodeGetInt(options, "region_max_x");
    int max_y = AiNodeGetInt(options, "region_max_y");
    
    // Setting Origin
    data->min_x = (min_x == INT_MIN) ? 0 : min_x;
    data->min_y = (min_y == INT_MIN) ? 0 : min_y;
    data->max_x = (max_x == INT_MIN) ? 0 : max_x;
    data->max_y = (max_y == INT_MIN) ? 0 : max_y;
   
    // Setting X Resolution
    if (data->min_x < 0 && data->max_x >= xres)
        data->xres = data->max_x - data->min_x + 1;
    else if (data->min_x >= 0 && data->max_x < xres)
        data->xres = xres;
    else if (data->min_x < 0 && data->max_x < xres)
        data->xres = xres - min_x;
    else if(data->min_x >= 0 && data->max_x >= xres)
        data->xres = xres + (max_x - xres + 1);
    
    // Setting Y Resolution
    if (data->min_y < 0 && data->max_y >= yres)
        data->yres = data->max_y - data->min_y + 1;
    else if (data->min_y >= 0 && data->max_y < yres)
        data->yres = yres;
    else if (data->min_y < 0 && data->max_y < yres)
        data->yres = yres - min_y;
    else if(data->min_y >= 0 && data->max_y >= yres)
        data->yres = yres + (max_y - yres + 1);
    
    // Get area of region
    long long rArea = data->xres * data->yres;

    boost::system::error_code ec;
    boost::asio::ip::address::from_string(extra_host, ec);
    data->extraHost = false;
    if (!ec) data->extraHost = true;
    
    try // Now we can connect to the server and start rendering
    {
        // Make image header & send to server
        aton::Data header(data->xres, data->yres, 0, 0, 0, 0,
                          rArea, version, currentFrame);
    
        data->client = new aton::Client(host, port);
        data->client->openImage(header);
        
        if (data->extraHost)
        {
            data->extra_client = new aton::Client(extra_host, port);
            data->extra_client->openImage(header);
        }
    }
    catch(const std::exception &e)
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
    
    if (data->min_x < 0)
        bucket_xo = bucket_xo - data->min_x;
    if (data->min_y < 0)
        bucket_yo = bucket_yo - data->min_y;
    
    while (AiOutputIteratorGetNext(iterator, &aov_name, &pixel_type, &bucket_data))
    {
        const float* ptr = reinterpret_cast<const float*>(bucket_data);
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
        aton::Data packet(data->xres, data->yres, bucket_xo, bucket_yo,
                          bucket_size_x, bucket_size_y,
                          0, 0, 0, spp, ram, time, aov_name, ptr);

        // Send it to the server
        data->client->sendPixels(packet);
        if (data->extraHost)
            data->extra_client->sendPixels(packet);
    }
}

driver_close
{
    AiMsgInfo("[Aton] driver close");

    ShaderData* data = (ShaderData*)AiDriverGetLocalData(node);
    try
    {
        data->client->closeImage();
        if (data->extraHost)
            data->extra_client->closeImage();
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
    if (data->extraHost)
        delete data->extra_client;
    
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
