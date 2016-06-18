/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FrameBuffer_h
#define FrameBuffer_h

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "DDImage/Iop.h"

using namespace DD::Image;

namespace aton
{
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
            RenderBuffer(unsigned int width = 0,
                         unsigned int height = 0,
                         int spp = 0);
        
            // Data
            std::vector<RenderColor> _color_data;
            std::vector<float> _float_data;
            unsigned int _width;
            unsigned int _height;
    };
    
    // Framebuffer main class
    class FrameBuffer
    {
        public:
            FrameBuffer(double currentFrame = 0, int w = 0, int h = 0);
        
            // Add new buffer
            void addBuffer(const char* aov = NULL, int spp = 0);
        
            // Set writable buffer's pixel
            float& setBufferPix(long b,
                                unsigned int x,
                                unsigned int y,
                                int spp,
                                int c);
        
            // Get read only buffer's pixel
            const float& getBufferPix(long b,
                                      unsigned int x,
                                      unsigned int y,
                                      int c) const;
        
            // Get the current buffer index
            long getBufferIndex(Channel z);
        
            // Get the current buffer index
            long getBufferIndex(const char * aovName);
        
            // Get N buffer/aov name name
            const char* getBufferName(size_t index = 0);
        
            // Get last buffer/aov name
            const std::string& getFirstBufferName();
        
            // Compare buffers with given buffer/aov names and dimensoions
            int compare(const unsigned int& width,
                        const unsigned int& height,
                        const double& frame,
                        const std::vector<std::string>& aovs);
        
            // Clear buffers and aovs
            void clearAll();
        
            // Check if the given buffer/aov name name is exist
            bool bufferNameExists(const char* aovName);
        
            // Set width of the buffer
            void setWidth(int w);

            // Set height of the buffer
            void setHeight(int h);
        
            // Get width of the buffer
            const int& getWidth();
        
            // Get height of the buffer
            const int& getHeight();

            // Get size of the buffers aka AOVs count
            size_t size();
        
            // Resize the buffers
            void resize(size_t s);
        
            // Set current bucket BBox for asapUpdate()
            void setBucketBBox(int x = 0, int y = 0, int r = 1, int t = 1);
        
            // Get current bucket BBox for asapUpdate()
            const Box& getBucketBBox();
        
            // Set status parameters
            void setProgress(long long progress = 0);
            void setRAM(long long ram = 0);
            void setTime(int time = 0);
        
            // Get status parameters
            const long long& getProgress();
            const long long& getRAM();
            const long long& getPRAM();
            const int& getTime();
        
            // Set Arnold core version
            void setArnoldVersion(int version);
        
            // Get Arnold core version
            const std::string& getArnoldVersion();
        
            // Set the frame number of this framebuffer
            void setFrame(double frame);
        
            // Get the frame number of this framebuffer
            const double& getFrame();
        
            // Check if this framebuffer is empty
            bool empty();
        
            // To keep False while writing the buffer
            void ready(bool ready);
            const bool& isReady();
        
        private:
            double _frame;
            long long _progress;
            int _time;
            long long _ram;
            long long _pram;
            int _width;
            int _height;
            Box _bucket;
            bool _ready;
            std::string _version;
            std::vector<RenderBuffer> _buffers;
            std::vector<std::string> _aovs;
    };
}

#endif /* FrameBuffer_h */
