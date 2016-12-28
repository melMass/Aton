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

void RenderColor::reset() { _val[0] = _val[1] = _val[2] = 0.0f; }

// RenderBuffer class
RenderBuffer::RenderBuffer(const unsigned int& width,
                           const unsigned int& height,
                           const int& spp)
{
    const int size = width * height;
    
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
void FrameBuffer::addBuffer(const char *aov,
                            const int& spp)
{
    RenderBuffer buffer(_width, _height, spp);
    
    _buffers.push_back(buffer);
    _aovs.push_back(aov);
}

// Get writable buffer object
void FrameBuffer::setBufferPix(const int& b,
                               const unsigned int& x,
                               const unsigned int& y,
                               const int& spp,
                               const int& c,
                               const float& pix)
{
    RenderBuffer& rb = _buffers[b];
    const unsigned int index = (_width * y) + x;
    if (c < 3 && spp != 1)
        rb._color_data[index][c] = pix;
    else
        rb._float_data[index] = pix;
}

// Get read only buffer object
const float& FrameBuffer::getBufferPix(const int& b,
                                       const unsigned int& x,
                                       const unsigned int& y,
                                       const int& c) const
{
    const RenderBuffer& rb = _buffers[b];
    const unsigned int index = (_width * y) + x;
    if (c < 3 && !rb._color_data.empty())
        return rb._color_data[index][c];
    else
        return rb._float_data[index];
}

// Get the current buffer index
int FrameBuffer::getBufferIndex(const Channel& z)
{
    int b_index = 0;
    if (_aovs.size() > 1)
    {
        using namespace chStr;
        const std::string& layer = getLayerName(z);

        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == layer)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
            else if (*it == Z && layer == depth)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }

    }
    return b_index;
}

// Get the current buffer index
int FrameBuffer::getBufferIndex(const char *aovName)
{
    int b_index = 0;
    if (_aovs.size() > 1)
    {
        std::vector<std::string>::iterator it;
        for(it = _aovs.begin(); it != _aovs.end(); ++it)
        {
            if (*it == aovName)
            {
                b_index = static_cast<int>(it - _aovs.begin());
                break;
            }
        }
    }
    return b_index;
}

// Get N buffer/aov name name
const char *FrameBuffer::getBufferName(const int& index)
{
    const char *aovName = "";

    if(!_aovs.empty())
    {
        try
        {
            aovName = _aovs.at(index).c_str();
        }
        catch (const std::out_of_range& e)
        {
            (void)e;
        }
        catch (...)
        {
            std::cerr << "Unexpected error at getting buffer name" << std::endl;
        }
    }
    
    return aovName;
}

// Get last buffer/aov name
bool FrameBuffer::isFirstBufferName(const char *aovName)
{
    return strcmp(_aovs.front().c_str(), aovName) == 0;;
}

// Check if Aovs has been changed
bool FrameBuffer::isAovsChanged(const std::vector<std::string>& aovs)
{
    return (aovs != _aovs);
}

// Check if Resolution has been changed
bool FrameBuffer::isResolutionChanged(const unsigned int& w,
                                      const unsigned int& h)
{
    return (w != _width || h != _height);
}

bool FrameBuffer::isCameraChanged(const float& fov,
                                  const Matrix4& matrix)
{
    return (_fov != fov || _matrix != matrix);
}

// Resize the containers to match the resolution
void FrameBuffer::setResolution(const unsigned int& w,
                                const unsigned int& h)
{
    _width = w;
    _height = h;
    
    const int bfSize = _width * _height;
    
    std::vector<RenderBuffer>::iterator iRB;
    for(iRB = _buffers.begin(); iRB != _buffers.end(); ++iRB)
    {
        if (!iRB->_color_data.empty())
        {
            RenderColor color;
            std::fill(iRB->_color_data.begin(), iRB->_color_data.end(), color);
            iRB->_color_data.resize(bfSize);
        }
        if (!iRB->_float_data.empty())
        {
            std::fill(iRB->_float_data.begin(), iRB->_float_data.end(), 0.0f);
            iRB->_float_data.resize(bfSize);
        }
    }
}

// Clear buffers and aovs
void FrameBuffer::clearAll()
{
    _buffers = std::vector<RenderBuffer>();
    _aovs = std::vector<std::string>();
}

// Check if the given buffer/aov name name is exist
bool FrameBuffer::isBufferExist(const char *aovName)
{
    return std::find(_aovs.begin(), _aovs.end(), aovName) != _aovs.end();
}

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
    const int ramGb = static_cast<int>(ram / 1048576);
    _ram = ramGb;
    _pram = ramGb > _pram ? ramGb : _pram;

}
void FrameBuffer::setTime(const int& time,
                          const int& dtime)
{
    _time = dtime > time ? time : time - dtime;
}

// Set Arnold core version
void FrameBuffer::setArnoldVersion(const int& version)
{
    // Construct a string from the version number passed
    const int archV = (version % 10000000) / 1000000;
    const int majorV = (version % 1000000) / 10000;
    const int minorV = (version % 10000) / 100;
    const int fixV = version % 100;
    
    std::stringstream stream;
    stream << archV << "." << majorV << "." << minorV << "." << fixV;
    _version = stream.str();
}

void FrameBuffer::setCamera(const float& fov, const Matrix4& matrix)
{
    _fov = fov;
    _matrix = matrix;
}

