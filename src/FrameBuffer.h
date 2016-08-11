/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FrameBuffer_h
#define FrameBuffer_h

#include "DDImage/Iop.h"

using namespace DD::Image;

namespace aton
{
    namespace chStr
    {
        extern const std::string RGBA, rgb, depth, Z, N, P,
                                 _red, _green, _blue, _X, _Y, _Z;
    }
    
    // Lightweight colour pixel class
    class RenderColor
    {
        public:
            RenderColor();

            float& operator[](int i);
            const float& operator[](int i) const;

            // Data
            float _val[3];
    };
    
    // Our image buffer class
    class RenderBuffer
    {
        friend class FrameBuffer;
        public:
            RenderBuffer(const unsigned int& width = 0,
                         const unsigned int& height = 0,
                         const int& spp = 0);
        
        private:
            // Data
            std::vector<RenderColor> _color_data;
            std::vector<float> _float_data;
    };
    
    // Framebuffer main class
    class FrameBuffer
    {
        public:
            FrameBuffer(const double& currentFrame = 0,
                        const int& w = 0,
                        const int& h = 0);
        
            // Add new buffer
            void addBuffer(const char* aov = NULL,
                           const int& spp = 0);
        
            // Set writable buffer's pixel
            void setBufferPix(const int& b,
                              const unsigned int& x,
                              const unsigned int& y,
                              const int& spp,
                              const int& c,
                              const float& pix);
        
            // Get read only buffer's pixel
            const float& getBufferPix(const int& b,
                                      const unsigned int& x,
                                      const unsigned int& y,
                                      const int& c) const;
        
            // Get the current buffer index
            int getBufferIndex(const Channel& z);
        
            // Get the current buffer index
            int getBufferIndex(const char * aovName);
        
            // Get N buffer/aov name name
            const char* getBufferName(const int& index);
        
            // Get last buffer/aov name
            bool isFirstBufferName(const char* aovName);
        
            // Check if Frame has been changed
            bool isFrameChanged(const double& frame);
        
            // Check if Aovs have been changed
            bool isAovsChanged(const std::vector<std::string>& aovs);
        
            // Check if Resolution has been changed
            bool isResolutionChanged(const unsigned int& width,
                                     const unsigned int& height);
        
            // Clear buffers and aovs
            void clearAll();
        
            // Check if the given buffer/aov name name is exist
            bool isBufferExist(const char* aovName);
        
            // Set width of the buffer
            void setWidth(const int& w);

            // Set height of the buffer
            void setHeight(const int& h);
        
            // Get width of the buffer
            const int& getWidth();
        
            // Get height of the buffer
            const int& getHeight();

            // Get size of the buffers aka AOVs count
            size_t size();
        
            // Resize the buffers
            void resize(const size_t& s);
        
            // Set status parameters
            void setProgress(const long long& progress = 0);
            void setRAM(const long long& ram = 0);
            void setTime(const int& time = 0,
                         const int& dtime = 0);
        
            // Get status parameters
            const long long& getProgress();
            const long long& getRAM();
            const long long& getPRAM();
            const int& getTime();
        
            // Set Arnold core version
            void setArnoldVersion(const int& version);
        
            // Get Arnold core version
            const char* getArnoldVersion();
        
            // Set the frame number of this framebuffer
            void setFrame(const double& frame);
        
            // Get the frame number of this framebuffer
            const double& getFrame();
        
            // Check if this framebuffer is empty
            bool empty();
        
            // To keep False while writing the buffer
            void ready(const bool& ready);
            const bool& isReady();
        
        private:
            double _frame;
            long long _progress;
            int _time;
            long long _ram;
            long long _pram;
            int _width;
            int _height;
            bool _ready;
            std::string _version;
            std::vector<RenderBuffer> _buffers;
            std::vector<std::string> _aovs;
    };
}

#endif /* FrameBuffer_h */
