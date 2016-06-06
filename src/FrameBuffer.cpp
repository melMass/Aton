/*
 Copyright (c) 2016,
 Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
 All rights reserved. See Copyright.txt for more details.
 */

#include "FrameBuffer.h"

using namespace aton;

// Lightweight colour pixel class
RenderColour::RenderColour() { _val[0] = _val[1] = _val[2] = 0.f; }

float& RenderColour::operator[](int i){ return _val[i]; }
const float& RenderColour::operator[](int i) const { return _val[i]; }

// Lightweight alpha pixel class
RenderAlpha::RenderAlpha() { _val = 1.f; }

float& RenderAlpha::operator[](int i){ return _val; }
const float& RenderAlpha::operator[](int i) const { return _val; }

// Our image buffer class
RenderBuffer::RenderBuffer(): _width(0), _height(0) {}

void RenderBuffer::initBuffer(const unsigned int width,
                              const unsigned int height,
                              const bool alpha)
{
    _width = width;
    _height = height;

    _colour_data.resize(_width * _height);
    if (alpha)
        _alpha_data.resize(_width * _height);
}

RenderColour& RenderBuffer::getColour(unsigned int x, unsigned int y)
{
    unsigned int index = (_width * y) + x;
    return _colour_data[index];
}

const RenderColour& RenderBuffer::getColour(unsigned int x, unsigned int y) const
{
    unsigned int index = (_width * y) + x;
    return _colour_data[index];
}

RenderAlpha& RenderBuffer::getAlpha(unsigned int x, unsigned int y)
{
    unsigned int index = (_width * y) + x;
    return _alpha_data[index];
}

const RenderAlpha& RenderBuffer::getAlpha(unsigned int x, unsigned int y) const
{
    unsigned int index = (_width * y) + x;
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
    
    if (spp < 4)
        buffer.initBuffer(_width, _height);
    else
        buffer.initBuffer(_width, _height, true);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get buffer object
RenderBuffer& FrameBuffer::getBuffer(int index)
{
    return _buffers[index];
}

// Get buffer object
const RenderBuffer& FrameBuffer::getBuffer(int index) const
{
    return _buffers[index];
}

// Get the current buffer index
int FrameBuffer::getBufferIndex(Channel z)
{
    int b_index = 0;
    
    if (!_aovs.empty())
    {
        std::string layer = getLayerName(z);
        if (layer.compare(ChannelStr::rgb) != 0)
        {
            for(std::vector<std::string>::iterator it = _aovs.begin(); it != _aovs.end(); ++it)
            {
                if (it->compare(layer) == 0)
                {
                    b_index = static_cast<int>(it - _aovs.begin());
                    break;
                }
                else if (it->compare(ChannelStr::Z) == 0 && layer.compare(ChannelStr::depth) == 0)
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
int FrameBuffer::getBufferIndex(const char* aovName)
{
    int b_index = 0;
    
    if (!_aovs.empty())
        for(std::vector<std::string>::iterator it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (it->compare(aovName) == 0)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }
    return b_index;
}

// Get N buffer/aov name name
std::string FrameBuffer::getBufferName(size_t index)
{
    std::string bufferName = "";
    if (!_aovs.empty())
        bufferName = _aovs[index];
    return bufferName;
}

// Get last buffer/aov name
std::string FrameBuffer::getFirstBufferName()
{
    std::string bufferName = "";
    if (!_aovs.empty())
        bufferName = _aovs.front();
    return bufferName;
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
int FrameBuffer::getWidth() { return _width; }

// Get height of the buffer
int FrameBuffer::getHeight() { return _height; }

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
Box FrameBuffer::getBucketBBox() { return _bucket; }

// Set status parameters
void FrameBuffer::setProgress(int progress) { _progress = progress; }
void FrameBuffer::setRAM(long long ram)
{
    _pram = _ram < ram ? ram : _ram;
    _ram = ram;
}
void FrameBuffer::setTime(int time) { _time = time; }

// Get status parameters
int FrameBuffer::getProgress() { return _progress; }
long long FrameBuffer::getRAM() { return _ram; }
long long FrameBuffer::getPRAM() { return _pram; }
int FrameBuffer::getTime() { return _time; }

// Set Arnold core version
void FrameBuffer::setArnoldVersion(int version)
{
    // Construct a string from the version number passed
    int archV = (version%10000000)/1000000;
    int majorV = (version%1000000)/10000;
    int minorV = (version%10000)/100;
    int fixV = version%100;
    _version = (boost::format("%s.%s.%s.%s")%archV%majorV%minorV%fixV).str();
}

// Get Arnold core version
std::string FrameBuffer::getArnoldVersion() { return _version; }

// Get the frame number of this framebuffer
double FrameBuffer::getFrame() { return _frame; }

// Check if this framebuffer is empty
bool FrameBuffer::empty() { return (_buffers.empty() && _aovs.empty()) ; }

// To keep False while writing the buffer
void FrameBuffer::ready(bool ready) { _ready = ready; }
bool FrameBuffer::isReady() { return _ready; }
