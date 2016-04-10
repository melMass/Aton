/*
 Copyright (c) 2016,
 Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
 All rights reserved. See Copyright.txt for more details.
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

#include "DDImage/Iop.h"
#include "DDImage/Row.h"
#include "DDImage/Thread.h"
#include "DDImage/Knobs.h"
#include "DDImage/DDMath.h"

using namespace DD::Image;

#include "Data.h"
#include "Server.h"

#include "boost/format.hpp"
#include "boost/foreach.hpp"
#include "boost/regex.hpp"
#include "boost/filesystem.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string.hpp"
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// class name
static const char* const CLASS = "Aton";

// version
static const char* const VERSION = "1.1.0b";

// help
static const char* const HELP =
    "Listens for renders coming from the Aton display driver. "
    "For more info go to http://sosoyan.github.io/Aton/";

// channel strings
namespace ChannelStr
{
    const std::string RGBA = "RGBA";
    const std::string rgb = "rgb";
    const std::string depth = "depth";
    const std::string Z = "Z";
    const std::string N = "N";
    const std::string P = "P";
};

// Our time change callback method
static void timeChange(unsigned index, unsigned nthreads, void* data);

// Our listener method
static void atonListen(unsigned index, unsigned nthreads, void* data);

// Lightweight colour pixel class
class RenderColour
{
    public:
        RenderColour()
        {
            _val[0] = _val[1] = _val[2] = 0.f;
        }

        float& operator[](int i){ return _val[i]; }
        const float& operator[](int i) const { return _val[i]; }

        // data
        float _val[3];
};

// Lightweight alpha pixel class
class RenderAlpha
{
    public:
        RenderAlpha()
        {
            _val = 1.f;
        }

        float& operator[](int i){ return _val; }
        const float& operator[](int i) const { return _val; }

        // data
        float _val;
};

// Our image buffer class
class RenderBuffer
{
    public:
        RenderBuffer() :
            _width(0),
            _height(0)
        {
        }

        void init(const unsigned int width, const unsigned int height, const bool empty = false, const bool alpha=false)
        {
            _width = width;
            _height = height;
            if (!empty)
            {
                _colour_data.resize(_width * _height);
                if (alpha)
                    _alpha_data.resize(_width * _height);
            }
        }

        RenderColour& getColour(unsigned int x, unsigned int y)
        {
            unsigned int index = (_width * y) + x;
            return _colour_data[index];
        }

        const RenderColour& getColour(unsigned int x, unsigned int y) const
        {
            unsigned int index = (_width * y) + x;
            return _colour_data[index];
        }

        RenderAlpha& getAlpha(unsigned int x, unsigned int y)
        {
            unsigned int index = (_width * y) + x;
            return _alpha_data[index];
        }

        const RenderAlpha& getAlpha(unsigned int x, unsigned int y) const
        {
            unsigned int index = (_width * y) + x;
            return _alpha_data[index];
        }
    
        unsigned int width() { return _width; }
        unsigned int height() { return _height; }
    
        bool empty()
        {
            return _colour_data.empty();
        }

        // data
        std::vector<RenderColour> _colour_data;
        std::vector<RenderAlpha> _alpha_data;
        unsigned int _width;
        unsigned int _height;
};

class FrameBuffer
{
    public:
        FrameBuffer(double currentFrame=0): _frame(0),
                                            _bucket(0,0,1,1),
                                            _progress(0),
                                            _time(0),
                                            _ram(0),
                                            _pram(0),
                                            _ready(false)
        {
            _frame = currentFrame;
        }
    
        // Add new buffer
        void addBuffer(const char * aov=NULL,
                       int spp=0,
                       int w=0,
                       int h=0)
        {
            RenderBuffer buffer;
            
            if (spp < 4)
                buffer.init(w, h);
            else
                buffer.init(w, h, false, true);
            
            _buffers.push_back(buffer);
            _aovs.push_back(aov);
        }
    
        // Get buffer object
        RenderBuffer& getBuffer(int index=0)
        {
            return _buffers[index];
        }
    
        // Get buffer object
        const RenderBuffer& getBuffer(int index=0) const
        {
            return _buffers[index];
        }
    
        // Get the current buffer index
        int getBufferIndex(Channel z)
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
                        else if ( it->compare(ChannelStr::Z) == 0 && layer.compare(ChannelStr::depth) == 0 )
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
        int getBufferIndex(const char * aovName)
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
        std::string getBufferName(size_t index=0)
        {
            std::string bufferName = "";
            if (!_aovs.empty())
                bufferName = _aovs[index];
            return bufferName;
        }
    
        // Get last buffer/aov name
        std::string getLastBufferName()
        {
            std::string bufferName = "";
            if (!_aovs.empty())
                bufferName = _aovs.back();
            return bufferName;
        }
    
        // Compare buffers with given buffer/aov names and dimensoions
        bool compareBuffers(int width, int height, std::vector<std::string> aovs)
        {
            return true ? (aovs == _aovs) &&
                          (width==_buffers[0].width()) &&
                          (height==_buffers[0].height()) : false;
        }
    
        // Clear buffers and aovs
        void clearBuffers()
        {
            _buffers.resize(0);
            _aovs.resize(0);
        }
    
        // Check if the given buffer/aov name name is exist
        bool bufferNameExists(const char * aovName)
        {
            return std::find(_aovs.begin(),
                             _aovs.end(),
                             aovName) != _aovs.end();
        }
    
        // Get width of the buffer
        int getWidth() { return _buffers[0].width(); }
    
        // Get height of the buffer
        int getHeight() { return _buffers[0].height(); }

        // Get size of the buffers aka AOVs count
        size_t size() { return _aovs.size(); }
    
        // Resize the buffers, useful for reseting
        void resize(size_t s)
        {
            _buffers.resize(s);
            _aovs.resize(s);
        }
    
        // Set current bucket BBox for asapUpdate()
        void setBucketBBox(int x=0, int y=0, int r=1, int t=1)
        {
            _bucket.set(x, y, r, t);
        }
    
        // Get current bucket BBox for asapUpdate()
        Box getBucketBBox() { return _bucket; }
    
        // Set status parameters
        void setProgress(int progress=0) { _progress = progress; }
        void setRAM(long long ram=0)
        {
            _pram = _ram < ram ? ram : _ram;
            _ram = ram;
        }
        void setTime(int time=0) { _time = time; }
    
        // Get status parameters
        int getProgress() { return _progress; }
        long long getRAM() { return _ram; }
        long long getPRAM() { return _pram; }
        int getTime() { return _time; }
    
        // Set Arnold core version
        void setArnoldVersion(int version)
        {
            // Construct a string from the version number passed
            int archV = (version%10000000)/1000000;
            int majorV = (version%1000000)/10000;
            int minorV = (version%10000)/100;
            int fixV = version%100;
            _version = (boost::format("%s.%s.%s.%s")%archV%majorV%minorV%fixV).str();
        }
    
        // Get Arnold core version
        std::string getArnoldVersion() { return _version; }
    
        // Get the frame number of this framebuffer
        double getFrame() { return _frame; }
    
        // Check if this framebuffer is empty
        bool empty() { return (_buffers.empty() && _aovs.empty()) ; }
    
        // To keep False while writing the buffer
        void ready(bool ready) { _ready = ready; }
        bool isReady() { return _ready; }
    
    private:
        double _frame;
        int _progress;
        int _time;
        long long _ram;
        long long _pram;
        Box _bucket;
        bool _ready;
        std::string _version;
        std::vector<RenderBuffer> _buffers;
        std::vector<std::string> _aovs;
};

// Nuke node
class Aton: public Iop
{
    public:
        Aton * m_node; // First node pointer
        const char * m_node_name; // Node name
        FormatPair m_fmtp; // Buffer format (knob)
        Format m_fmt; // The nuke display format
        int m_port; // Port we're listening on (knob)
        const char * m_path; // Default path for Write node
        std::string m_status; // Status bar text
        bool m_multiframe_cache;
        bool m_all_frames;
        double m_current_frame;
        const char * m_comment;
        bool m_stamp;
        bool m_enable_aovs;
        int m_stamp_size;
        int m_slimit; // The limit size
        RenderBuffer m_buffer; // Blank buffer
        Lock m_mutex; // Mutex for locking the pixel buffer
        unsigned int hash_counter; // Refresh hash counter
        aton::Server m_server; // Aton::Server
        bool m_inError; // Error handling
        bool m_formatExists;
        bool m_capturing; // Capturing signal
        std::vector<std::string> m_garbageList;
        std::vector<double> m_frames; // Frames holder
        std::vector<FrameBuffer> m_framebuffers; // Framebuffers holder
        std::string m_connectionError;
        ChannelSet m_channels;
        bool m_legit;

        Aton(Node* node): Iop(node),
                          m_node(firstNode()),
                          m_port(getPort()),
                          m_path(getPath()),
                          m_status(""),
                          m_multiframe_cache(true),
                          m_all_frames(false),
                          m_current_frame(0),
                          m_comment(""),
                          m_stamp(true),
                          m_enable_aovs(true),
                          m_stamp_size(15),
                          m_slimit(20),
                          m_fmt(Format(0, 0, 1.0)),
                          m_inError(false),
                          m_formatExists(false),
                          m_capturing(false),
                          m_connectionError(""),
                          m_channels(Mask_RGBA),
                          m_legit(false)
        {
            inputs(0);
        }

        ~Aton()
        {
            disconnect();
        }
    
        Aton* firstNode()
        {
            return dynamic_cast<Aton*>(firstOp());
        }

        // It seems additional instances of a node get copied/constructed upon
        // very frequent calls to asapUpdate() and this causes us a few
        // problems - we don't want new sockets getting opened etc.
        // Fortunately attach() only gets called for nodes in the dag so we can
        // use this to mark the DAG node as 'legit' and open the port accordingly.
        void attach()
        {
            m_legit = true;

            // default status bar
            setStatus();

            // We don't need to see these knobs
            knob("formats_knob")->hide();
            knob("capturing_knob")->hide();

            // Allocate node name in order to pass it to format
            char * format = new char[node_name().length() + 1];
            strcpy(format, node_name().c_str());
            m_node_name = format;
            
            // Running python code to check if we've already our format in the script
            std::string cmd; // Our python command buffer
            
            cmd = (boost::format("bool([i.name() for i in nuke.formats() if i.name()=='%s'])")%m_node_name).str();
            script_command(cmd.c_str());
            std::string result = script_result();
            script_unlock();
            
            // Checking if the format is already exist
            if (result.compare("True") != 0)
                m_fmt.add(m_node_name);
            else
                m_formatExists = true;
        }

        void detach()
        {
            // even though a node still exists once removed from a scene (in the
            // undo stack) we should close the port and reopen if attach() gets
            // called.
            m_legit = false;
            disconnect();
            m_node->m_frames.resize(0);
            m_node->m_framebuffers.resize(0);
            
            delete[] m_path;
            m_path = NULL;
        }

        void flagForUpdate(int f_index)
        {
            if ( hash_counter==UINT_MAX )
                hash_counter=0;
            else
                hash_counter++;
            
            // Update the image with current bucket first
            asapUpdate(m_node->m_framebuffers[f_index].getBucketBBox());
        }

        // we can use this to change our tcp port
        void changePort( int port )
        {
            m_inError = false;
            m_connectionError = "";
            
            // try to reconnect
            disconnect();

            try
            {
                m_server.connect(port, true);
            }
            catch ( ... )
            {
                std::stringstream ss;
                ss << "Could not connect to port: " << port;
                m_connectionError = ss.str();
                m_inError = true;
                print_name( std::cerr );
                std::cerr << ": " << ss.str() << std::endl;
                return;
            }

            // success
            if ( m_server.isConnected() )
            {
                Thread::spawn(::atonListen, 1, this);
                Thread::spawn(::timeChange, 1, this);
                print_name( std::cout );
                
                // Update port in the UI
                std::string cmd; // Our python command buffer
                cmd = (boost::format("nuke.toNode('%s')['port_number'].setValue(%s)")%m_node_name
                                                                                     %m_server.getPort()).str();
                script_command(cmd.c_str());
                script_unlock();
                
                std::cout << ": Connected to port " << m_server.getPort() << std::endl;
            }
        }

        // disconnect the server for it's port
        void disconnect()
        {
            if ( m_server.isConnected() )
            {
                m_server.quit();
                Thread::wait(this);

                print_name( std::cout );
                std::cout << ": Disconnected from port " << m_server.getPort() << std::endl;
            }
        }

        void append(Hash& hash)
        {
            hash.append(m_node->hash_counter);
            hash.append(m_node->outputContext().frame());
            hash.append(m_node->uiContext().frame());
        }

        void _validate(bool for_real)
        {
            // Do we need to open a port?
            if ( m_server.isConnected()==false && !m_inError && m_legit )
                changePort(m_port);
            
            // Handle any connection error
            if ( m_inError )
                error(m_connectionError.c_str());

            if (!m_node->m_framebuffers.empty())
            {
                // Get the frame and set the format
                int f_index = getFrameIndex(uiContext().frame());
                
                FrameBuffer &frameBuffer = m_node->m_framebuffers[f_index];

                if (!frameBuffer.empty())
                {
                    if (frameBuffer.getProgress() > 0)
                        setStatus(frameBuffer.getProgress(),
                                  frameBuffer.getRAM(),
                                  frameBuffer.getPRAM(),
                                  frameBuffer.getTime(),
                                  frameBuffer.getFrame(),
                                  frameBuffer.getArnoldVersion());
                    
                    // Set the format
                    int width = frameBuffer.getWidth();
                    int height = frameBuffer.getHeight();
                    
                    if (m_node->m_fmt.width() != width ||
                        m_node->m_fmt.height() != height)
                    {
                        Format *m_fmt_exist = &m_node->m_fmt;
                        if (m_node->m_formatExists)
                        {
                            // If the format is already exist we need to get its pointer
                            for (int i=0; i < Format::size(); i++)
                            {
                                m_fmt_exist = Format::index(i);
                                if (std::string(m_fmt_exist->name()).compare(m_node->m_node_name) == 0)
                                    break;
                            }
                        }
                        
                        m_fmt_exist->set(0, 0, width, height);
                        m_fmt_exist->width(width);
                        m_fmt_exist->height(height);
                        knob("formats_knob")->set_text(m_node->m_node_name);
                    }

                    size_t fb_size = frameBuffer.size();
                    ChannelSet& channels = m_node->m_channels;
                    
                    if (channels.size() != fb_size)
                        channels.clear();

                    for(int i = 0; i < fb_size; ++i)
                    {
                        std::string bufferName = frameBuffer.getBufferName(i);
                        
                        if (bufferName.compare(ChannelStr::RGBA)==0)
                        {
                            if (!channels.contains(Chan_Red))
                            {
                                channels.insert(Chan_Red);
                                channels.insert(Chan_Green);
                                channels.insert(Chan_Blue);
                                channels.insert(Chan_Alpha);
                            }
                        }
                        else if (bufferName.compare(ChannelStr::Z)==0)
                        {
                            if (!channels.contains(Chan_Z))
                                channels.insert( Chan_Z );
                        }
                        else if (bufferName.compare(ChannelStr::N)==0 ||
                                 bufferName.compare(ChannelStr::P)==0)
                        {
                            if (!channels.contains(channel((boost::format("%s.X")%bufferName.c_str()).str().c_str())))
                            {
                                channels.insert(channel((boost::format("%s.X")%bufferName.c_str()).str().c_str()));
                                channels.insert(channel((boost::format("%s.Y")%bufferName.c_str()).str().c_str()));
                                channels.insert(channel((boost::format("%s.Z")%bufferName.c_str()).str().c_str()));
                            }
                        }
                        else
                        {
                            if (!channels.contains(channel((boost::format("%s.red")%bufferName.c_str()).str().c_str())))
                            {
                                channels.insert(channel((boost::format("%s.red")%bufferName.c_str()).str().c_str()));
                                channels.insert(channel((boost::format("%s.blue")%bufferName.c_str()).str().c_str()));
                                channels.insert(channel((boost::format("%s.green")%bufferName.c_str()).str().c_str()));
                            }
                        }
                    }
                }
            }
            
            // Disable caching
            m_node->slowness(0);
            
            // Setup format etc
            info_.format(*m_node->m_fmtp.fullSizeFormat());
            info_.full_size_format(*m_node->m_fmtp.format());
            info_.channels( m_node->m_channels );
            info_.set(m_node->info().format());
        }

        void engine(int y, int xx, int r, ChannelMask channels, Row& out)
        {
            int f_index = getFrameIndex(uiContext().frame());

            foreach(z, channels)
            {
                int b_index = 0;
                if (!m_node->m_framebuffers.empty())
                    b_index = m_node->m_framebuffers[f_index].getBufferIndex(z);

                float *rOut = out.writable(brother (z, 0)) + xx;
                float *gOut = out.writable(brother (z, 1)) + xx;
                float *bOut = out.writable(brother (z, 2)) + xx;
                float *aOut = out.writable(Chan_Alpha) + xx;
                const float *END = rOut + (r - xx);
                unsigned int xxx = static_cast<unsigned int> (xx);
                unsigned int yyy = static_cast<unsigned int> (y);

                std::vector<FrameBuffer>& fbs  = m_node->m_framebuffers;
                
                m_mutex.lock();
                while (rOut < END)
                {
                    if (fbs.empty() || !fbs[f_index].isReady() ||
                        xxx >= fbs[f_index].getWidth() ||
                        yyy >= fbs[f_index].getHeight())
                    {
                        *rOut = *gOut = *bOut = *aOut = 0.f;
                    }
                    else
                    {
                        *rOut = fbs[f_index].getBuffer(b_index).getColour(xxx, yyy)[0];
                        *gOut = fbs[f_index].getBuffer(b_index).getColour(xxx, yyy)[1];
                        *bOut = fbs[f_index].getBuffer(b_index).getColour(xxx, yyy)[2];
                        *aOut = fbs[f_index].getBuffer(0).getAlpha(xxx, yyy)[0];
                    }
                    ++rOut;
                    ++gOut;
                    ++bOut;
                    ++aOut;
                    ++xxx;
                }
                m_mutex.unlock();
            }
        }

        void knobs(Knob_Callback f)
        {
            // Hidden knobs
            Format_knob(f, &m_fmtp, "formats_knob", "format");
            Bool_knob(f, &m_capturing, "capturing_knob");

            // Main knobs
            Int_knob(f, &m_port, "port_number", "Port");
            Spacer(f, 10000);
            Text_knob(f, (boost::format("Aton v%s")%VERSION).str().c_str());

            Divider(f, "General");
            Knob * enable_aovs_knob = Bool_knob(f, &m_enable_aovs,
                                                "enable_aovs_knob",
                                                "Enable AOVs");
            Newline(f);
            Knob * sync_current_frame_knob = Bool_knob(f, &m_multiframe_cache,
                                                       "multi_frame_cache_knob",
                                                       "Multi Frame Caching");

            Divider(f, "Capture");
            Knob * limit_knob = Int_knob(f, &m_slimit, "limit_knob", "Limit");
            Newline(f);
            Knob * all_frames_knob = Bool_knob(f, &m_all_frames,
                                               "all_frames_knob",
                                               "Capture all frames");
            Knob * path_knob = File_knob(f, &m_path, "path_knob", "Path");

            Newline(f);
            Knob * stamp_knob = Bool_knob(f, &m_stamp, "stamp_knob", "Frame Stamp");
            Knob * stamp_size_knob = Int_knob(f, &m_stamp_size, "stamp_size_knob", "Size");
            Knob * comment_knob = String_knob(f, &m_comment, "comment_knob", "Comment");
            Newline(f);
            Button(f, "capture_knob", "Capture");
            Button(f, "import_latest_knob", "Import latest");
            Button(f, "import_all_knob", "Import all");

            // Status Bar knobs
            BeginToolbar(f, "status_bar");
            Knob * statusKnob = String_knob(f, &m_status, "status_knob", "");
            EndToolbar(f);

            // Set Flags
            enable_aovs_knob->set_flag(Knob::NO_RERENDER, true);
            sync_current_frame_knob->set_flag(Knob::NO_RERENDER, true);
            limit_knob->set_flag(Knob::NO_RERENDER, true);
            path_knob->set_flag(Knob::NO_RERENDER, true);
            all_frames_knob->set_flag(Knob::NO_RERENDER, true);
            stamp_knob->set_flag(Knob::NO_RERENDER, true);
            stamp_size_knob->set_flag(Knob::NO_RERENDER, true);
            comment_knob->set_flag(Knob::NO_RERENDER, true);

            statusKnob->set_flag(Knob::NO_RERENDER, true);
            statusKnob->set_flag(Knob::DISABLED, true);
            statusKnob->set_flag(Knob::OUTPUT_ONLY, true);
        }

        int knob_changed(Knob* _knob)
        {
            if (_knob->is("port_number"))
            {
                changePort(m_port);
                return 1;
            }
            if (_knob->is("capture_knob"))
            {
                captureCmd();
                return 1;
            }
            if (_knob->is("use_stamp_knob"))
            {
                if(!m_stamp)
                {
                    knob("stamp_size_knob")->enable(false);
                    knob("comment_knob")->enable(false);
                }
                else
                {
                    knob("stamp_size_knob")->enable(true);
                    knob("comment_knob")->enable(true);
                }
                return 1;
            }
            if (_knob->is("import_latest_knob"))
            {
                importLatestCmd();
                return 1;
            }
            if (_knob->is("import_all_knob"))
            {
                importAllCmd();
                return 1;
            }
            return 0;
        }
    
        void setCurrentFrame(double frame)
        {
            std::string cmd; // Our python command buffer
            
            // Create a Write node and return it's name
            cmd = (boost::format("nuke.frame(%s)")%frame).str();
            script_command(cmd.c_str());
            script_unlock();
        }
    
        int getFrameIndex(double currentFrame)
        {
            int f_index = 0;

            if (m_node->m_multiframe_cache && !m_node->m_frames.empty())
            {
                int nearFIndex = INT_MIN;
                int minFIndex = INT_MAX;
                
                for(std::vector<double>::iterator it = m_node->m_frames.begin();
                    it != m_node->m_frames.end(); ++it)
                {
                    if (currentFrame == *it)
                    {
                        f_index = static_cast<int>(it - m_node->m_frames.begin());
                        break;
                    }
                    else if (currentFrame > *it && nearFIndex < *it)
                    {
                        nearFIndex = *it;
                        f_index = static_cast<int>(it - m_node->m_frames.begin());
                        continue;
                    }
                    else if (*it < minFIndex && nearFIndex == INT_MIN)
                    {
                        minFIndex = *it;
                        f_index = static_cast<int>(it - m_node->m_frames.begin());
                    }
                }
            }
            return f_index;
        }
    
        char * getPath()
        {
            char * aton_path;
            std::string def_path;

            aton_path = getenv("ATON_CAPTURE_PATH");

            if (aton_path == NULL)
            {
                // Get OS specific tmp directory path
                def_path = boost::filesystem::temp_directory_path().string();
            }
            else def_path = aton_path;

            boost::replace_all(def_path, "\\", "/");

            // Construct the full path for Write node
            boost::filesystem::path dir = def_path;
            boost::filesystem::path file = "Aton.exr";
            boost::filesystem::path fullPath = dir / file;

            std::string str_path = fullPath.string();
            boost::replace_all(str_path, "\\", "/");

            char * full_path = new char[str_path.length()+1];
            strcpy(full_path, str_path.c_str());

            return full_path;
        }
    
        int getPort()
        {
            char * aton_port;
            int def_port = 9201;
            
            aton_port = getenv("ATON_PORT");
            
            if (aton_port != NULL)
                def_port = atoi(aton_port);
            
            return def_port;
        }

        std::string getDateTime()
        {
            // Returns date and time
            time_t rawtime;
            struct tm * timeinfo;
            char time_buffer[20];

            time (&rawtime);
            timeinfo = localtime (&rawtime);

            // Setting up the Date and Time format style
            strftime(time_buffer, 20, "%Y-%m-%d_%H-%M-%S", timeinfo);

            std::string path = std::string(m_path);
            std::string key (".");

            return std::string(time_buffer);
        }

        std::vector<std::string> getCaptures()
        {
            // Our captured filenames list
            std::vector<std::string> results;

            boost::filesystem::path filepath(m_path);
            boost::filesystem::directory_iterator it(filepath.parent_path());
            boost::filesystem::directory_iterator end;

            // Regex expression to find captured files
            std::string exp = ( boost::format("%s.+.%s")%filepath.stem().string()
                                                        %filepath.extension().string() ).str();
            const boost::regex filter(exp);

            // Iterating through directory to find matching files
            BOOST_FOREACH(boost::filesystem::path const &p, std::make_pair(it, end))
            {
                if(boost::filesystem::is_regular_file(p))
                {
                    boost::match_results<std::string::const_iterator> what;
                    if (boost::regex_search(it->path().filename().string(), what, filter, boost::match_default))
                    {
                        std::string res = p.filename().string();
                        results.push_back(res);
                    }
                }
            }
            return results;
        }

        void cleanByLimit()
        {
            if ( !m_garbageList.empty() )
            {
                // in windows sometimes files can't be deleted due to lack of
                // access so we collecting a garbage list and trying to remove
                // them next time when user make a capture
                for(std::vector<std::string>::iterator it = m_garbageList.begin();
                    it != m_garbageList.end(); ++it)
                {
                    std::remove(it->c_str());
                }
            }

            int count = 0;
            std::vector<std::string> captures = getCaptures();
            boost::filesystem::path filepath(m_path);
            boost::filesystem::path dir = filepath.parent_path();

            // Reverse iterating through file list
            if ( !captures.empty() )
            {
                for(std::vector<std::string>::reverse_iterator it = captures.rbegin();
                    it != captures.rend(); ++it)
                {
                    boost::filesystem::path file = *it;
                    boost::filesystem::path path = dir / file;
                    std::string str_path = path.string();
                    boost::replace_all(str_path, "\\", "/");

                    count += 1;

                    // Remove the file if it's out of limit
                    if (count >= m_slimit)
                    {
                        if (std::remove(str_path.c_str()) != 0)
                            m_garbageList.push_back(str_path);

                        std::string cmd; // Our python command buffer

                        // Remove appropriate Read nodes as well
                        cmd = ( boost::format("exec('''for i in nuke.allNodes('Read'):\n\t"
                                                          "if '%s' == i['file'].value():\n\t\t"
                                                              "nuke.delete(i)''')")%str_path ).str();
                        script_command(cmd.c_str(), true, false);
                        script_unlock();
                    }
                }
            }
        }

        void captureCmd()
        {
            if  (m_slimit != 0)
            {
                // Get the path and add time date suffix to it
                std::string key (".");
                std::string path = std::string(m_path);
                std::string timeFrameSuffix;
                std::string frames;
                double startFrame;
                double endFrame;

                if (m_node->m_frames.size() > 0)
                {
                    std::vector<double> sortedFrames = m_node->m_frames;
                    std::stable_sort(sortedFrames.begin(), sortedFrames.end());

                    if (m_multiframe_cache && m_all_frames)
                    {
                        timeFrameSuffix += "_" + std::string("####");
                        startFrame = sortedFrames.front();
                        endFrame = sortedFrames.back();
                        for(std::vector<double>::iterator it = sortedFrames.begin();
                                                          it != sortedFrames.end(); ++it)
                            frames += (boost::format("%s,")%*it).str();
                        frames.resize(frames.size() - 1);
                    }
                    else
                    {
                        timeFrameSuffix += "_" + getDateTime();
                        startFrame = endFrame = uiContext().frame();
                        frames = (boost::format("%s")%uiContext().frame()).str();
                    }
                }
                else
                    return;

                timeFrameSuffix += ".";
                std::size_t found = path.rfind(key);
                if (found!=std::string::npos)
                    path.replace(found, key.length(), timeFrameSuffix);

                std::string cmd; // Our python command buffer

                // Create a Write node and return it's name
                cmd = (boost::format("nuke.nodes.Write(file='%s').name()")%path.c_str()).str();
                script_command(cmd.c_str());
                std::string writeNodeName = script_result();
                script_unlock();

                // Connect to Write node
                cmd = (boost::format("nuke.toNode('%s').setInput(0, nuke.toNode('%s'));"
                                     "nuke.toNode('%s')['channels'].setValue('all');"
                                     "nuke.toNode('%s')['afterRender']."
                                     "setValue('''nuke.nodes.Read(file='%s', first=%s, last=%s, on_error=3)''')")%writeNodeName
                                                                                                                 %m_node->m_node_name
                                                                                                                 %writeNodeName
                                                                                                                 %writeNodeName
                                                                                                                 %path.c_str()
                                                                                                                 %startFrame
                                                                                                                 %endFrame).str();
                script_command(cmd.c_str(), true, false);
                script_unlock();
                
                if (m_stamp)
                {
                    // Create a rectangle node and return it's name
                    cmd = (boost::format("nuke.nodes.Rectangle(opacity=0.95, color=0.05).name()")).str();
                    script_command(cmd.c_str());
                    std::string RectNodeName = script_result();
                    script_unlock();

                    // Set the rectangle size
                    cmd = (boost::format("rect = nuke.toNode('%s');"
                                         "rect['output'].setValue('rgb');"
                                         "rect['area'].setValue([0,0,%s,%s]);"
                                         "rect.setInput(0, nuke.toNode('%s'))")%RectNodeName
                                                                               %(m_node->m_fmt.width()+1000)
                                                                               %(m_node->m_stamp_size+7)
                                                                               %m_node->m_node_name).str();
                    script_command(cmd.c_str(), true, false);
                    script_unlock();

                    // Add text node in between to put a stamp on the capture
                    cmd = (boost::format("stamp = nuke.nodes.Text();"
                                         "stamp['message'].setValue('''[python {nuke.toNode('%s')['status_knob'].value()}] | Comment: %s''');"
                                         "stamp['yjustify'].setValue('bottom');"
                                         "stamp['size'].setValue(%s);"
                                         "stamp['output'].setValue('rgb');"
                                         "stamp['font'].setValue(nuke.defaultFontPathname());"
                                         "stamp['color'].setValue(0.5);"
                                         "stamp['translate'].setValue([5, 2.5]);"
                                         "stamp.setInput(0, nuke.toNode('%s'));"
                                         "nuke.toNode('%s').setInput(0, stamp)")%m_node->m_node_name%m_node->m_comment
                                                                                %m_node->m_stamp_size%RectNodeName
                                                                                %writeNodeName ).str();
                    script_command(cmd.c_str(), true, false);
                    script_unlock();
                }

                // Execute the Write node
                cmd = (boost::format("exec('''import thread\n"
                                             "def writer():\n\t"
                                                 "def status(b):\n\t\t"
                                                     "nuke.toNode('%s')['capturing_knob'].setValue(b)\n\t\t"
                                                     "if not b:\n\t\t\t"
                                                         "if %s:\n\t\t\t\t"
                                                            "nuke.delete(nuke.toNode('%s').input(0).input(0))\n\t\t\t\t"
                                                            "nuke.delete(nuke.toNode('%s').input(0))\n\t\t\t"
                                                         "nuke.delete(nuke.toNode('%s'))\n\t"
                                                 "nuke.executeInMainThread(status, args=True)\n\t"
                                                 "nuke.executeInMainThread(nuke.execute, args=('%s', nuke.FrameRanges([%s])))\n\t"
                                                 "nuke.executeInMainThread(status, args=False)\n"
                                             "thread.start_new_thread(writer,())''')")%m_node->m_node_name
                                                                                      %m_stamp
                                                                                      %writeNodeName
                                                                                      %writeNodeName
                                                                                      %writeNodeName
                                                                                      %writeNodeName
                                                                                      %frames).str();
                script_command(cmd.c_str(), true, false);
                script_unlock();
            }
            cleanByLimit();
        }

        void importLatestCmd()
        {
            std::vector<std::string> captures = getCaptures();
            boost::filesystem::path filepath(m_path);
            boost::filesystem::path dir = filepath.parent_path();

            if ( !captures.empty() )
            {
                // Getting last ellemnt of the vector
                boost::filesystem::path file = captures.back();
                boost::filesystem::path path = dir / file;
                std::string str_path = path.string();
                boost::replace_all(str_path, "\\", "/");

                std::string cmd; // Our python command buffer

                cmd = ( boost::format("exec('''readNodes = nuke.allNodes('Read')\n"
                                              "exist = False\n"
                                              "if len(readNodes)>0:\n\t"
                                                  "for i in readNodes:\n\t\t"
                                                      "if '%s' == i['file'].value():\n\t\t\t"
                                                          "exist = True\n"
                                               "if exist != True:\n\t"
                                               "nuke.nodes.Read(file='%s')''')")%str_path
                                                                                %str_path ).str();
                script_command(cmd.c_str(), true, false);
                script_unlock();
            }
        }

        void importAllCmd()
        {
            std::vector<std::string> captures = getCaptures();
            boost::filesystem::path filepath(m_path);
            boost::filesystem::path dir = filepath.parent_path();

            if ( !captures.empty() )
            {
                // Reverse iterating through vector
                for(std::vector<std::string>::reverse_iterator it = captures.rbegin();
                    it != captures.rend(); ++it)
                {
                    boost::filesystem::path file = *it;
                    boost::filesystem::path path = dir / file;
                    std::string str_path = path.string();
                    boost::replace_all(str_path, "\\", "/");

                    std::string cmd; // Our python command buffer

                    cmd = ( boost::format("exec('''readNodes = nuke.allNodes('Read')\n"
                                                  "exist = False\n"
                                                  "if len(readNodes)>0:\n\t"
                                                      "for i in readNodes:\n\t\t"
                                                          "if '%s' == i['file'].value():\n\t\t\t"
                                                              "exist = True\n"
                                                   "if exist != True:\n\t"
                                                      "nuke.nodes.Read(file='%s')''')")%str_path
                                                                                       %str_path ).str();
                    script_command(cmd.c_str(), true, false);
                    script_unlock();
                }
            }
        }
    
        std::string setStatus(int progress=0,
                              long long ram=0,
                              long long p_ram=0,
                              int time=0,
                              double frame=0,
                              std::string version="")
        {
            ram /= 1024*1024;
            p_ram /= 1024*1024;

            int hour = time / (1000*60*60);
            int minute = (time % (1000*60*60)) / (1000*60);
            int second = ((time % (1000*60*60)) % (1000*60)) / 1000;

            if (progress>100) progress=100;

            std::string str_status = (boost::format("Arnold: %s | "
                                                    "Used Memory: %sMB | "
                                                    "Peak Memory: %sMB | "
                                                    "Time: %02ih:%02im:%02is | "
                                                    "Frame: %04i | "
                                                    "Progress: %s%%")%version%ram%p_ram
                                                                     %hour%minute%second
                                                                     %frame%progress).str();
            knob("status_knob")->set_text(str_status.c_str());
            return str_status;
        }
    
        bool firstEngineRendersWholeRequest() const { return true; }
        const char* Class() const { return CLASS; }
        const char* displayName() const { return CLASS; }
        const char* node_help() const { return HELP; }
        static const Iop::Description desc;
};

// Time change thread method
static void timeChange(unsigned index, unsigned nthreads, void* data)
{
    Aton * node = reinterpret_cast<Aton*> (data);
    std::vector<FrameBuffer>& fbs  = node->m_node->m_framebuffers;

    double prevFrame = 0;
    int milliseconds = 10;

    while (node->m_legit)
    {
        if (!fbs.empty() && prevFrame != node->uiContext().frame())
        {
            node->flagForUpdate(node->uiContext().frame());
            prevFrame = node->uiContext().frame();
        }
        boost::this_thread::sleep(boost::posix_time::millisec(milliseconds));
    }
}

// Listening thread method
static void atonListen(unsigned index, unsigned nthreads, void* data)
{
    bool killThread = false;
    std::vector<std::string> active_aovs;

    Aton * node = reinterpret_cast<Aton*> (data);

    while (!killThread)
    {
        // Accept incoming connections!
        node->m_server.accept();

        // Our incoming data object
        aton::Data d;

        // For progress percentage
        long long imageArea = 0;
        int progress = 0;
        int f_index = 0;
        
        // For time to reset per every IPR iteration
        static int active_time = 0;
        static int delta_time = 0;

        // Loop over incoming data
        while ((d.type()==2||d.type()==9)==false)
        {
            // Listen for some data
            try
            {
                d = node->m_server.listen();
            }
            catch( ... )
            {
                break;
            }

            // Handle the data we received
            switch (d.type())
            {
                case 0: // Open a new image
                {
                    // Init blank buffer
                    node->m_mutex.lock();
                    node->m_buffer.init(d.width(), d.height(), true);
                    
                    double _active_frame = static_cast<double>(d.currentFrame());
                    
                    // Init current frame
                    if (node->m_current_frame != _active_frame)
                        node->m_current_frame = _active_frame;
                    
                    // Sync timeline
                    if (node->m_multiframe_cache)
                    {
                        if (std::find(node->m_frames.begin(),
                                      node->m_frames.end(),
                                      _active_frame) == node->m_frames.end())
                        {
                            FrameBuffer frameBuffer(_active_frame);
                            node->m_frames.push_back(_active_frame);
                            node->m_framebuffers.push_back(frameBuffer);
                        }
                    }
                    else
                    {
                        if (node->m_frames.empty())
                        {
                            FrameBuffer frameBuffer(_active_frame);
                            node->m_frames.push_back(_active_frame);
                            node->m_framebuffers.push_back(frameBuffer);
                        }
                        else if (node->m_frames.size() > 1)
                        {
                            node->m_frames.resize(1);
                            node->m_framebuffers.resize(1);
                        }
                    }
                    node->m_mutex.unlock();
                    
                    // Get frame buffer
                    f_index = node->getFrameIndex(_active_frame);
                    FrameBuffer &frameBuffer = node->m_framebuffers[f_index];
                    
                    // Reset Buffers anc Channels
                    if (!active_aovs.empty() && !frameBuffer.empty() &&
                        !frameBuffer.compareBuffers(d.width(),
                                                    d.height(),
                                                    active_aovs))
                    {
                        node->m_mutex.lock();
                        frameBuffer.clearBuffers();
                        frameBuffer.ready(false);
                        if (node->m_channels.size()>4)
                        {
                            node->m_channels.clear();
                            node->m_channels.insert(Chan_Red);
                            node->m_channels.insert(Chan_Green);
                            node->m_channels.insert(Chan_Blue);
                            node->m_channels.insert(Chan_Alpha);
                        }
                        node->m_mutex.unlock();
                    }
                    
                    // Get image area to calculate the progress
                    if (d.width()*d.height() == d.rArea())
                        imageArea = d.width()*d.height();
                    else
                        imageArea = d.rArea();
                    
                    // Get delta time per IPR iteration
                    delta_time = active_time;
                    
                    // Set Arnold Core version
                    frameBuffer.setArnoldVersion(d.version());
                    
                    // Set time to current frame
                    if (node->m_multiframe_cache &&
                        node->uiContext().frame() != node->m_current_frame)
                        node->setCurrentFrame(node->m_current_frame);
                    
                    // Reset active AOVs
                    if(!active_aovs.empty())
                        active_aovs.clear();
                    
                    break;
                }
                case 1: // image data
                {
                    // copy data from d into node->m_buffer
                    int _w = node->m_buffer._width;
                    int _h = node->m_buffer._height;
                    unsigned int _x, _x0, _y, _y0, _s, offset;
                    _x = _x0 = _y = _y0 = _s = 0;

                    int _xorigin = d.x();
                    int _yorigin = d.y();
                    int _width = d.width();
                    int _height = d.height();
                    int _spp = d.spp();
                    long long _ram = d.ram();
                    int _time = d.time();
                    
                    // Get active time
                    active_time = d.time();

                    // Get active aov names
                    if(!(std::find(active_aovs.begin(),
                                   active_aovs.end(),
                                   d.aovName()) != active_aovs.end()))
                    {
                        if (node->m_enable_aovs)
                            active_aovs.push_back(d.aovName());
                        else
                        {
                            if (active_aovs.size()==0)
                                active_aovs.push_back(d.aovName());
                            else if (active_aovs.size() > 1)
                                active_aovs.resize(1);
                        }
                    }
                    
                    // Get frame buffer
                    FrameBuffer &frameBuffer = node->m_framebuffers[f_index];
                    
                    // Lock buffer
                    node->m_mutex.lock();
                    
                    // Adding buffer
                    if(!frameBuffer.bufferNameExists(d.aovName()))
                    {
                        if (node->m_enable_aovs || frameBuffer.size()==0)
                            frameBuffer.addBuffer(d.aovName(), _spp, _w, _h);
                    }
                    else
                        frameBuffer.ready(true);
                    
                    // Get buffer index
                    int b_index = frameBuffer.getBufferIndex(d.aovName());
                    
                    // Writing to buffer
                    const float* pixel_data = d.pixels();
                    for (_x = 0; _x < _width; ++_x)
                        for (_y = 0; _y < _height; ++_y)
                        {
                            offset = (_width * _y * _spp) + (_x * _spp);

                            RenderColour &pix = frameBuffer.getBuffer(b_index).getColour(_x+ _xorigin, _h - (_y + _yorigin + 1));
                            for (_s = 0; _s < _spp; ++_s)
                                if (_s != 3)
                                    pix[_s] = pixel_data[offset+_s];
                            if (_spp == 4)
                            {
                                RenderAlpha &alpha_pix = frameBuffer.getBuffer(b_index).getAlpha(_x+ _xorigin, _h - (_y + _yorigin + 1));
                                alpha_pix[0] = pixel_data[offset+3];
                            }
                        }

                    // Release lock
                    node->m_mutex.unlock();

                    // Skip while capturing
                    if (node->m_capturing)
                        continue;

                    // Update only on first aov
                    if(frameBuffer.getLastBufferName().compare(d.aovName()) == 0)
                    {
                        // Calculating the progress percentage
                        imageArea -= (_width*_height);
                        progress = static_cast<int>(100 - (imageArea*100) / (_w * _h));

                        // Getting redraw bucket size
                        node->m_mutex.lock();
                        frameBuffer.setBucketBBox(_xorigin,
                                                  _h - _yorigin - _height,
                                                  _xorigin + _width,
                                                  _h - _yorigin);

                        // Setting status parameters
                        frameBuffer.setProgress(progress);
                        frameBuffer.setRAM(_ram);
                        if (delta_time > _time)
                            frameBuffer.setTime(_time);
                        else
                            frameBuffer.setTime(_time - delta_time);
                        node->m_mutex.unlock();
                        
                        // Update the image
                        node->flagForUpdate(f_index);
                    }

                    // Deallocate aov name
                    d.clearAovName();
                    break;
                }
                case 2: // Close image
                {
                    break;
                }
                case 9: // This is sent when the parent process want to kill
                        // the listening thread
                {
                    killThread = true;
                    break;
                }
            }
        }
    }
}
//=====
// nuke builder stuff
static Iop* constructor(Node* node){ return new Aton(node); }
const Iop::Description Aton::desc(CLASS, 0, constructor);
