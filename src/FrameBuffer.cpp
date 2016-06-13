/*
 Copyright (c) 2016,
 Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
 All rights reserved. See Copyright.txt for more details.
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

// Lightweight colour pixel class
RenderColour::RenderColour() { _val[0] = _val[1] = _val[2] = 0.f; }

float& RenderColour::operator[](int i){ return _val[i]; }
const float& RenderColour::operator[](int i) const { return _val[i]; }

// Our image buffer class
RenderBuffer::RenderBuffer(): _width(0), _height(0) {}

void RenderBuffer::initBuffer(const unsigned int width,
                              const unsigned int height,
                              const unsigned int spp)
{
    _width = width;
    _height = height;
    
    switch (spp)
    {
        case 1:
        {
            _alpha_data.resize(_width * _height);
            break;
        }
        case 3:
        {
            _colour_data.resize(_width * _height);
            break;
        }
        case 4:
        {
            _colour_data.resize(_width * _height);
            _alpha_data.resize(_width * _height);
            break;
        }
    }
}

float& RenderBuffer::getColour(unsigned int x,
                               unsigned int y,
                               int c,
                               int spp)
{
    unsigned int index = (_width * y) + x;
    
    if ((spp == 3 || spp == 4) && c < 3)
        return _colour_data[index][c];
    else
        return _alpha_data[index];
}

const float& RenderBuffer::getColour(unsigned int x,
                                     unsigned int y,
                                     int c) const
{
    unsigned int index = (_width * y) + x;
    
    if (c == 0 && _colour_data.empty())
        return _alpha_data[index];
    else if (c < 3)
        return _colour_data[index][c];
    else
        return _alpha_data[index];
}

unsigned int RenderBuffer::width() { return _width; }
unsigned int RenderBuffer::height() { return _height; }

bool RenderBuffer::empty()
{
    return _colour_data.empty();
}

// Framebuffer main class
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
    RenderBuffer buffer;
    
    buffer.initBuffer(_width, _height, spp);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get writable buffer object
float& FrameBuffer::getBufferPix(long b,
                                 unsigned int x,
                                 unsigned int y,
                                 int c,
                                 int spp)
{
    return _buffers[b].getColour(x, y, c, spp);
}

// Get read only buffer object
const float& FrameBuffer::getBufferPix(long b,
                                       unsigned int x,
                                       unsigned int y,
                                       int c) const
{
    return _buffers[b].getColour(x, y, c);
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
                    b_index = static_cast<int>(it - _aovs.begin());
                    break;
                }
                else if (it->compare(Z) == 0 && layer.compare(depth) == 0)
                {
                    b_index = static_cast<int>(it - _aovs.begin());
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
        for(std::vector<std::string>::iterator it = _aovs.begin();
                                               it != _aovs.end(); ++it)
        {
            if (it->compare(aovName) == 0)
            {
                b_index = it - _aovs.begin();
                break;
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
int FrameBuffer::compareAll(int width, 
                            int height, 
                            std::vector<std::string> aovs)
{
    if (!_buffers.empty() && !_aovs.empty())
    {
        if (aovs == _aovs &&
            width == _buffers[0].width() &&
            height == _buffers[0].height())
            return 0;
        else if (width == _buffers[0].width() &&
                 height == _buffers[0].height())
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
void FrameBuffer::setProgress(long long progress) { _progress = progress; }
void FrameBuffer::setRAM(long long ram)
{
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
