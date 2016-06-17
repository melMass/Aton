/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "FrameBuffer.h"
#include "boost/format.hpp"

namespace chStr
{
    const std::string rgb = "rgb",
                     rgba = "rgba",
                    depth = "depth",
                        Z = "Z";
}

using namespace aton;

// Lightweight color pixel class
RenderColor::RenderColor() { _val[0] = _val[1] = _val[2] = 0.0f; }

float& RenderColor::operator[](int i){ return _val[i]; }
const float& RenderColor::operator[](int i) const { return _val[i]; }


// RenderBuffer class
RenderBuffer::RenderBuffer(unsigned int width,
                           unsigned int height,
                           int spp)
{
    _width = width;
    _height = height;
    
    switch (spp)
    {
        case 1:
        {
            _float_data.resize(_width * _height);
            break;
        }
        case 3:
        {
            _color_data.resize(_width * _height);
            break;
        }
        case 4:
        {
            _color_data.resize(_width * _height);
            _float_data.resize(_width * _height);
            break;
        }
    }
}

bool RenderBuffer::empty()
{
    return _color_data.empty();
}


// FrameBuffer class
FrameBuffer::FrameBuffer(double currentFrame,
                         int w,
                         int h): _frame(0),
                                 _bucket(0,0,1,1),
                                 _progress(0),
                                 _time(0),
                                 _ram(0),
                                 _pram(0),
                                 _ready(false)
{
    _frame = currentFrame;
    _width = w;
    _height = h;
}

// Add new buffer
void FrameBuffer::addBuffer(const char* aov, int spp)
{
    RenderBuffer buffer(_width, _height, spp);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get writable buffer object
float& FrameBuffer::setBufferPix(long b,
                                 unsigned int x,
                                 unsigned int y,
                                 int spp,
                                 int c)
{
    RenderBuffer& rb = _buffers[b];
    unsigned int index = (rb._width * y) + x;
    if (c < 3 && spp != 1)
        return rb._color_data[index][c];
    else
        return rb._float_data[index];
}

// Get read only buffer object
const float& FrameBuffer::getBufferPix(long b,
                                       unsigned int x,
                                       unsigned int y,
                                       int c) const
{
    const RenderBuffer& rb = _buffers[b];
    unsigned int index = (rb._width * y) + x;
    if (c < 3 && !rb._color_data.empty())
        return rb._color_data[index][c];
    else
        return rb._float_data[index];
}

// Get the current buffer index
long FrameBuffer::getBufferIndex(Channel z)
{
    long b_index = 0;    
    if (!_aovs.empty())
    {
        using namespace chStr;
        std::string layer = getLayerName(z);
        if (layer.compare(rgb) != 0)
        {
            std::vector<std::string>::iterator it;
            for(it = _aovs.begin(); it != _aovs.end(); ++it)
            {
                if (it->compare(layer) == 0)
                {
                    b_index = it - _aovs.begin();
                    break;
                }
                else if (it->compare(Z) == 0 && layer.compare(depth) == 0)
                {
                    b_index = it - _aovs.begin();
                    break;
                }
            }
        }
    }
    return b_index;
}

// Get the current buffer index
long FrameBuffer::getBufferIndex(const char* aovName)
{
    long b_index = 0;
    if (!_aovs.empty())
    {
        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (it->compare(aovName) == 0)
            {
                b_index = it - _aovs.begin();
                break;
            }
        }
    }
    return b_index;
}

// Get N buffer/aov name name
const char* FrameBuffer::getBufferName(size_t index)
{
    return _aovs[index].c_str();
}

// Get last buffer/aov name
const std::string& FrameBuffer::getFirstBufferName()
{
    return _aovs.front();
}

// Compare buffers with given buffer/aov names and dimensoions
int FrameBuffer::compareAll(const unsigned int& width,
                            const unsigned int& height,
                            const std::vector<std::string>& aovs)
{
    if (!_buffers.empty() && !_aovs.empty())
    {
        if (aovs == _aovs &&
            width == _buffers[0]._width &&
            height == _buffers[0]._height)
            return 0;
        else if (width == _buffers[0]._width &&
                 height == _buffers[0]._height)
            return 1;
        else
            return 2;
    }
    else
        return -1;
}

// Clear buffers and aovs
void FrameBuffer::clearAll()
{
    _buffers = std::vector<RenderBuffer>();
    _aovs = std::vector<std::string>();
}

// Check if the given buffer/aov name name is exist
bool FrameBuffer::bufferNameExists(const char* aovName)
{
    return std::find(_aovs.begin(),
                     _aovs.end(),
                     aovName) != _aovs.end();
}

// Set width of the buffer
void FrameBuffer::setWidth(int w) { _width = w; }

// Set height of the buffer
void FrameBuffer::setHeight(int h) { _height = h; }

// Get width of the buffer
const int& FrameBuffer::getWidth() { return _width; }

// Get height of the buffer
const int& FrameBuffer::getHeight() { return _height; }

// Get size of the buffers aka AOVs count
size_t FrameBuffer::size() { return _aovs.size(); }

// Resize the buffers
void FrameBuffer::resize(size_t s)
{
    _buffers.resize(s);
    _aovs.resize(s);
}

// Set current bucket BBox for asapUpdate()
void FrameBuffer::setBucketBBox(int x, int y, int r, int t)
{
    _bucket.set(x, y, r, t);
}

// Get current bucket BBox for asapUpdate()
const Box& FrameBuffer::getBucketBBox() { return _bucket; }

// Set status parameters
void FrameBuffer::setProgress(long long progress)
{
    _progress = progress > 100 ? 100 : progress;
}

void FrameBuffer::setRAM(long long ram)
{
    ram /= 1048576;
    _pram = _ram < ram ? ram : _ram;
    _ram = ram;
}
void FrameBuffer::setTime(int time) { _time = time; }

// Get status parameters
const long long& FrameBuffer::getProgress() { return _progress; }
const long long& FrameBuffer::getRAM() { return _ram; }
const long long& FrameBuffer::getPRAM() { return _pram; }
const int& FrameBuffer::getTime() { return _time; }

// Set Arnold core version
void FrameBuffer::setArnoldVersion(int version)
{
    // Construct a string from the version number passed
    int archV = (version % 10000000) / 1000000;
    int majorV = (version % 1000000) / 10000;
    int minorV = (version % 10000) / 100;
    int fixV = version % 100;
    _version = (boost::format("%s.%s.%s.%s")%archV%majorV%minorV%fixV).str();
}

// Get Arnold core version
const std::string& FrameBuffer::getArnoldVersion() { return _version; }

// Get the frame number of this framebuffer
const double& FrameBuffer::getFrame() { return _frame; }

// Check if this framebuffer is empty
bool FrameBuffer::empty() { return (_buffers.empty() && _aovs.empty()) ; }

// To keep False while writing the buffer
void FrameBuffer::ready(bool ready) { _ready = ready; }
const bool& FrameBuffer::isReady() { return _ready; }
