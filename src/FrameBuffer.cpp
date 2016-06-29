/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#include "FrameBuffer.h"
#include "boost/format.hpp"

using namespace aton;

const std::string chStr::RGBA = "RGBA",
                  chStr::rgb = "rgb",
                  chStr::depth = "depth",
                  chStr::Z = "Z",
                  chStr::N = "N",
                  chStr::P = "P",
                  chStr::_red = ".red",
                  chStr::_green = ".green",
                  chStr::_blue = ".blue",
                  chStr::_X = ".X",
                  chStr::_Y = ".Y",
                  chStr::_Z = ".Z";

// Lightweight color pixel class
RenderColor::RenderColor() { _val[0] = _val[1] = _val[2] = 0.0f; }

float& RenderColor::operator[](int i) { return _val[i]; }
const float& RenderColor::operator[](int i) const { return _val[i]; }

// RenderBuffer class
RenderBuffer::RenderBuffer(const unsigned int& width,
                           const unsigned int& height,
                           const int& spp)
{
    _width = width;
    _height = height;
    size_t size = width * height;
    
    switch (spp)
    {
        case 1: // Float channels
            _float_data.resize(size);
            break;
        case 3: // Color Channels
            _color_data.resize(size);
            break;
        case 4: // Color + Alpha channels
            _color_data.resize(size);
            _float_data.resize(size);
            break;
    }
}

// FrameBuffer class
FrameBuffer::FrameBuffer(const double& currentFrame,
                         const int& w,
                         const int& h): _frame(currentFrame),
                                        _width(w),
                                        _height(h),
                                        _progress(0),
                                        _time(0),
                                        _ram(0),
                                        _pram(0),
                                        _ready(false) {}

// Add new buffer
void FrameBuffer::addBuffer(const char* aov,
                            const int& spp)
{
    RenderBuffer buffer(_width, _height, spp);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get writable buffer object
float& FrameBuffer::setBufferPix(const long& b,
                                 const unsigned int& x,
                                 const unsigned int& y,
                                 const int& spp,
                                 const int& c)
{
    RenderBuffer& rb = _buffers[b];
    unsigned int index = (rb._width * y) + x;
    if (c < 3 && spp != 1)
        return rb._color_data[index][c];
    else
        return rb._float_data[index];
}

// Get read only buffer object
const float& FrameBuffer::getBufferPix(const long& b,
                                       const unsigned int& x,
                                       const unsigned int& y,
                                       const int& c) const
{
    const RenderBuffer& rb = _buffers[b];
    unsigned int index = (rb._width * y) + x;
    if (c < 3 && !rb._color_data.empty())
        return rb._color_data[index][c];
    else
        return rb._float_data[index];
}

// Get the current buffer index
long FrameBuffer::getBufferIndex(const Channel& z)
{
    long b_index = 0;    
    if (_aovs.size() > 1)
    {
        using namespace chStr;
        std::string layer = getLayerName(z);
        if (layer != rgb)
        {
            std::vector<std::string>::iterator it;
            for(it = _aovs.begin(); it != _aovs.end(); ++it)
            {
                if (*it == layer)
                {
                    b_index = it - _aovs.begin();
                    break;
                }
                else if (*it == Z && layer == depth)
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
    if (_aovs.size() > 1)
    {
        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == aovName)
            {
                b_index = it - _aovs.begin();
                break;
            }
        }
    }
    return b_index;
}

// Get N buffer/aov name name
const char* FrameBuffer::getBufferName(const size_t& index)
{
    return _aovs[index].c_str();
}

// Get last buffer/aov name
bool FrameBuffer::isFirstBufferName(const char* aovName)
{
    return strcmp(_aovs.front().c_str(), aovName) == 0;;
}

// Check if Frame has been changed
bool FrameBuffer::isFrameChanged(const double& frame)
{
    return frame != _frame;
}

// Check if Aovs has been changed
bool FrameBuffer::isAovsChanged(const std::vector<std::string>& aovs)
{
    return (aovs != _aovs);
}

// Check if Resolution has been changed
bool FrameBuffer::isResolutionChanged(const unsigned int& width,
                                      const unsigned int& height)
{
    return (width != _buffers[0]._width &&
            height != _buffers[0]._height);
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
                     _aovs.end(), aovName) != _aovs.end();
}

// Set width of the buffer
void FrameBuffer::setWidth(const int& w) { _width = w; }

// Set height of the buffer
void FrameBuffer::setHeight(const int& h) { _height = h; }

// Get width of the buffer
const int& FrameBuffer::getWidth() { return _width; }

// Get height of the buffer
const int& FrameBuffer::getHeight() { return _height; }

// Get size of the buffers aka AOVs count
size_t FrameBuffer::size() { return _aovs.size(); }

// Resize the buffers
void FrameBuffer::resize(const size_t& s)
{
    _buffers.resize(s);
    _aovs.resize(s);
}

// Set status parameters
void FrameBuffer::setProgress(const long long& progress)
{
    _progress = progress > 100 ? 100 : progress;
}

void FrameBuffer::setRAM(const long long& ram)
{
    int ramGb = static_cast<int>(ram / 1048576);
    _pram = ramGb > _ram ? ramGb : _ram;
    _ram = ramGb;
}
void FrameBuffer::setTime(const int& time,
                          const int& dtime)
{
    _time = dtime > time ? time : time - dtime;
}

// Get status parameters
const long long& FrameBuffer::getProgress() { return _progress; }
const long long& FrameBuffer::getRAM() { return _ram; }
const long long& FrameBuffer::getPRAM() { return _pram; }
const int& FrameBuffer::getTime() { return _time; }

// Set Arnold core version
void FrameBuffer::setArnoldVersion(const int& version)
{
    // Construct a string from the version number passed
    int archV = (version % 10000000) / 1000000;
    int majorV = (version % 1000000) / 10000;
    int minorV = (version % 10000) / 100;
    int fixV = version % 100;
    
    std::stringstream stream;
    stream << archV << "." << majorV << "." << minorV << "." << fixV;
    _version = stream.str();
}

// Get Arnold core version
const char* FrameBuffer::getArnoldVersion() { return _version.c_str(); }

// Set the frame number of this framebuffer
void FrameBuffer::setFrame(const double& frame) { _frame = frame; }

// Get the frame number of this framebuffer
const double& FrameBuffer::getFrame() { return _frame; }

// Check if this framebuffer is empty
bool FrameBuffer::empty() { return (_buffers.empty() && _aovs.empty()) ; }

// To keep False while writing the buffer
void FrameBuffer::ready(const bool& ready) { _ready = ready; }
const bool& FrameBuffer::isReady() { return _ready; }
