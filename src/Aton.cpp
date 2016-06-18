/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
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
#include "FrameBuffer.h"

using namespace aton;

#include "boost/format.hpp"
#include "boost/foreach.hpp"
#include "boost/regex.hpp"
#include "boost/filesystem.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/thread/thread.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

// Class name
static const char* const CLASS = "Aton";

// Version
static const char* const VERSION = "1.1.3";

// Help
static const char* const HELP =
    "Listens for renders coming from the Aton display driver. "
    "For more info go to http://sosoyan.github.io/Aton/";

// Our time change callback method
static void timeChange(unsigned index, unsigned nthreads, void* data);

// Our listener method
static void atonListen(unsigned index, unsigned nthreads, void* data);

namespace chStr
{
    const std::string RGBA = "RGBA",
                       rgb = "rgb",
                     depth = "depth",
                         Z = "Z",
                         N = "N",
                         P = "P",
                      _red = ".red",
                    _green = ".green",
                     _blue = ".blue",
                        _X = ".X",
                        _Y = ".Y",
                        _Z = ".Z";
}

// Nuke node
class Aton: public Iop
{
    public:
        Aton*                     m_node;             // First node pointer
        const char*               m_node_name;        // Node name
        FormatPair                m_fmtp;             // Buffer format (knob)
        Format                    m_fmt;              // The nuke display format
        int                       m_port;             // Port we're listening on (knob)
        const char*               m_path;             // Default path for Write node
        std::string               m_status;           // Status bar text
        bool                      m_multiframes;      // Enable Multiple Frames toogle
        bool                      m_all_frames;       // Capture All Frames toogle
        const char*               m_comment;          // Comment for the frame stamp
        bool                      m_stamp;            // Enable Frame stamp toogle
        bool                      m_enable_aovs;      // Enable AOVs toogle
        double                    m_stamp_scale;      // Frame stamp size
        int                       m_slimit;           // The limit size
        Lock                      m_mutex;            // Mutex for locking the pixel buffer
        unsigned int              m_hash_count;       // Refresh hash counter
        Server                    m_server;           // Aton::Server
        bool                      m_inError;          // Error handling
        bool                      m_formatExists;     // If the format was already exist
        bool                      m_capturing;        // Capturing signal
        std::vector<std::string>  m_garbageList;      // List of captured files to be deleted
        std::vector<double>       m_frames;           // Frames holder
        std::vector<FrameBuffer>  m_framebuffers;     // Framebuffers holder
        std::string               m_connectionError;  // Connection error report
        ChannelSet                m_channels;         // Channels aka AOVs object
        bool                      m_legit;            // Used to throw the threads

        Aton(Node* node): Iop(node),
                          m_node(firstNode()),
                          m_port(getPort()),
                          m_path(""),
                          m_status(""),
                          m_multiframes(true),
                          m_all_frames(false),
                          m_comment(""),
                          m_stamp(isVersionValid()),
                          m_enable_aovs(true),
                          m_stamp_scale(1.0),
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
            
            // Disable caching
            m_node->slowness(0);

            // Default status bar
            setStatus();

            // We don't need to see these knobs
            knob("formats_knob")->hide();
            knob("capturing_knob")->hide();
            
            if (!isVersionValid())
            {
                knob("stamp_knob")->enable(false);
                knob("stamp_scale_knob")->enable(false);
                knob("comment_knob")->enable(false);
            }
            
            // Allocate node name in order to pass it to format
            char* nodeName = new char[node_name().length() + 1];
            strcpy(nodeName, node_name().c_str());
            m_node_name = nodeName;
            
            // Construct full path for capturing
            using namespace boost::filesystem;
            path dir = getPath();
            path file = (boost::format("%s.exr")%m_node_name).str();
            path fullPath = dir / file;
            std::string str_path = fullPath.string();
            boost::replace_all(str_path, "\\", "/");
            knob("path_knob")->set_text(str_path.c_str());
            
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
            // Even though a node still exists once removed from a scene (in the
            // undo stack) we should close the port and reopen if attach() gets
            // called.
            m_legit = false;
            disconnect();
            m_node->m_frames = std::vector<double>();
            m_node->m_framebuffers = std::vector<FrameBuffer>();
            
            delete[] m_path;
            m_path = NULL;
        }

        void flagForUpdate(long f_index = 0)
        {
            if (m_hash_count == UINT_MAX)
                m_hash_count = 0;
            else
                m_hash_count++;
            
            // Update the image with current bucket if given
            asapUpdate(m_node->m_framebuffers[f_index].getBucketBBox());
        }

        // We can use this to change our tcp port
        void changePort(int port)
        {
            m_inError = false;
            m_legit = false;
            m_connectionError = "";
            
            // Try to reconnect
            disconnect();

            try
            {
                m_node->m_server.connect(port, true);
                m_legit = true;
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

            // Success
            if (m_server.isConnected())
            {
                Thread::spawn(::atonListen, 1, this);
                Thread::spawn(::timeChange, 1, this);
                print_name(std::cout);
                
                // Update port in the UI
                if (m_node->m_port != m_node->m_server.getPort())
                {
                    std::stringstream str;
                    str << (m_node->m_server.getPort());
                    std::string port = str.str();
                    knob("port_number")->set_text(port.c_str());
                }
            }
        }

        // Disconnect the server for it's port
        void disconnect()
        {
            if (m_server.isConnected())
            {
                m_server.quit();
                Thread::wait(this);
            }
        }

        void append(Hash& hash)
        {
            hash.append(m_node->m_hash_count);
            hash.append(uiContext().frame());
        }

        void _validate(bool for_real)
        {
            // Do we need to open a port?
            if (m_node->m_server.isConnected() == false && !m_inError && m_legit)
                changePort(m_port);
            
            // Handle any connection error
            if (m_inError)
                error(m_connectionError.c_str());

            if (!m_node->m_framebuffers.empty())
            {
                long f_index = getFrameIndex(uiContext().frame());
                FrameBuffer& fB = m_node->m_framebuffers[f_index];
                
                if (!fB.empty())
                {
                    // Set the progress
                    if (fB.getProgress() > 0)
                        setStatus(fB.getProgress(),
                                  fB.getRAM(),
                                  fB.getPRAM(),
                                  fB.getTime(),
                                  fB.getFrame(),
                                  fB.getArnoldVersion());
                    
                    // Set the format
                    const int& width = fB.getWidth();
                    const int& height = fB.getHeight();
                    
                    if (m_node->m_fmt.width() != width ||
                        m_node->m_fmt.height() != height)
                    {
                        // If the format is already exist
                        Format* m_fmt_exist = &m_node->m_fmt;
                        if (m_node->m_formatExists)
                        {
                            for (int i=0; i < Format::size(); ++i)
                            {
                                const char* f_name = Format::index(i)->name();
                                if (strcmp(f_name, m_node->m_node_name) == 0)
                                    m_fmt_exist = Format::index(i);
                            }
                        }
                        
                        m_fmt_exist->set(0, 0, width, height);
                        m_fmt_exist->width(width);
                        m_fmt_exist->height(height);
                        knob("formats_knob")->set_text(m_node->m_node_name);
                    }
                    
                    // Set the channels
                    ChannelSet& channels = m_node->m_channels;
                    
                    if (m_enable_aovs)
                    {
                        size_t fb_size = fB.size();
                        
                        if (channels.size() != fb_size)
                            channels.clear();

                        for(int i = 0; i < fb_size; ++i)
                        {
                            using namespace chStr;
                            const std::string& bfName = fB.getBufferName(i);
                            
                            if (bfName.compare(RGBA) == 0 && !channels.contains(Chan_Red))
                            {
                                channels.insert(Chan_Red);
                                channels.insert(Chan_Green);
                                channels.insert(Chan_Blue);
                                channels.insert(Chan_Alpha);
                            }
                            else if (bfName.compare(Z) == 0 && !channels.contains(Chan_Z))
                            {
                                channels.insert(Chan_Z);
                            }
                            else if (bfName.compare(N) == 0 || bfName.compare(P) == 0)
                            {
                                if (!channels.contains(channel((bfName + _X).c_str())))
                                {
                                    channels.insert(channel((bfName + _X).c_str()));
                                    channels.insert(channel((bfName + _Y).c_str()));
                                    channels.insert(channel((bfName + _Z).c_str()));
                                }
                            }
                            else if (!channels.contains(channel((bfName + _red).c_str())))
                            {
                                channels.insert(channel((bfName + _red).c_str()));
                                channels.insert(channel((bfName + _green).c_str()));
                                channels.insert(channel((bfName + _blue).c_str()));
                            }
                        }
                    }
                    else
                        resetChannels(channels);
                }
            }
            
            // Setup format etc
            info_.format(*m_node->m_fmtp.fullSizeFormat());
            info_.full_size_format(*m_node->m_fmtp.format());
            info_.channels( m_node->m_channels );
            info_.set(m_node->info().format());
        }

        void engine(int y, int x, int r, ChannelMask channels, Row& out)
        {
            std::vector<FrameBuffer>& fBs = m_node->m_framebuffers;
            long f = getFrameIndex(uiContext().frame());
            unsigned int xx = static_cast<unsigned int>(x);
            
            foreach(z, channels)
            {
                long b = 0;
                if (m_enable_aovs && !fBs.empty() && fBs[f].isReady())
                    b = fBs[f].getBufferIndex(z);
                
                x = xx;
                int c = colourIndex(z);
                float* cOut = out.writable(z) + x;
                const float* END = cOut + (r - x);
                
                m_mutex.lock();
                while (cOut < END)
                {
                    if (fBs.empty() || !fBs[f].isReady() ||
                        x >= fBs[f].getWidth() ||
                        y >= fBs[f].getHeight())
                    {
                        *cOut = 0.0f;
                    }
                    else
                        *cOut = fBs[f].getBufferPix(b, x, y, c);
                    ++cOut;
                    ++x;
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
            Button(f, "clear_all_knob", "Clear All");
            Spacer(f, 10000);
            Text_knob(f, (boost::format("Aton v%s")%VERSION).str().c_str());

            Divider(f, "General");
            Bool_knob(f, &m_enable_aovs, "enable_aovs_knob", "Enable AOVs");
            Newline(f);
            Bool_knob(f, &m_multiframes, "multi_frame_knob", "Enable Multiple Frames");

            Divider(f, "Capture");
            Knob* limit_knob = Int_knob(f, &m_slimit, "limit_knob", "Limit");
            Knob* all_frames_knob = Bool_knob(f, &m_all_frames, "all_frames_knob", "Capture All Frames");
            Knob* path_knob = File_knob(f, &m_path, "path_knob", "Path");

            Newline(f);
            Knob* stamp_knob = Bool_knob(f, &m_stamp, "stamp_knob", "Frame Stamp");
            Knob* stamp_scale_knob = Float_knob(f, &m_stamp_scale, "stamp_scale_knob", "Scale");
            Knob* comment_knob = String_knob(f, &m_comment, "comment_knob", "Comment");
            Newline(f);
            Button(f, "capture_knob", "Capture");
            Button(f, "import_latest_knob", "Import latest");
            Button(f, "import_all_knob", "Import all");

            // Status Bar knobs
            BeginToolbar(f, "status_bar");
            Knob* statusKnob = String_knob(f, &m_status, "status_knob", "");
            EndToolbar(f);

            // Set Flags
            limit_knob->set_flag(Knob::NO_RERENDER, true);
            path_knob->set_flag(Knob::NO_RERENDER, true);
            all_frames_knob->set_flag(Knob::NO_RERENDER, true);
            stamp_knob->set_flag(Knob::NO_RERENDER, true);
            stamp_scale_knob->set_flag(Knob::NO_RERENDER, true);
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
            if (_knob->is("clear_all_knob"))
            {
                clearAllCmd();
                return 1;
            }
            if (_knob->is("capture_knob"))
            {
                captureCmd();
                return 1;
            }
            if (_knob->is("stamp_knob"))
            {
                if(!m_stamp)
                {
                    knob("stamp_scale_knob")->enable(false);
                    knob("comment_knob")->enable(false);
                }
                else
                {
                    knob("stamp_scale_knob")->enable(true);
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
    
        void resetChannels(ChannelSet& channels)
        {
            if (channels.size() > 4)
            {
                channels.clear();
                channels.insert(Chan_Red);
                channels.insert(Chan_Green);
                channels.insert(Chan_Blue);
                channels.insert(Chan_Alpha);
            }
        }
    
        bool isVersionValid()
        {
            // Check the Nuke version to be minimum 9.0v7 in order
            // to status stamp text be consistant with Linux version
            std::string cmd = "nuke.NUKE_VERSION_MAJOR * 100 + "
                              "nuke.NUKE_VERSION_MINOR * 10 + "
                              "nuke.NUKE_VERSION_RELEASE >= 907";
            script_command(cmd.c_str());
            std::string result = script_result();
            script_unlock();
                
            return (result.compare("True") == 0);
        }
    
        bool isPathValid(std::string path)
        {
            boost::filesystem::path filepath(path);
            boost::filesystem::path dir = filepath.parent_path();
            return boost::filesystem::exists(dir);
        }
    
        void setCurrentFrame(double frame)
        {
            // Set Current Frame and update the UI
            OutputContext ctxt = outputContext();
            ctxt.setFrame(frame);
            gotoContext(ctxt, true);
        }
    
        long getFrameIndex(double currentFrame)
        {
            long f_index = 0;
            std::vector<double>& frames = m_node->m_frames;

            if (frames.size() > 1)
            {
                if (m_multiframes)
                {
                    int nearFIndex = INT_MIN;
                    int minFIndex = INT_MAX;
                    std::vector<double>::iterator it;
                    for( it = frames.begin(); it != frames.end(); ++it)
                    {
                        if (currentFrame == *it)
                        {
                            f_index = it - frames.begin();
                            break;
                        }
                        else if (currentFrame > *it && nearFIndex < *it)
                        {
                            nearFIndex = static_cast<int>(*it);
                            f_index = it - frames.begin();
                            continue;
                        }
                        else if (*it < minFIndex && nearFIndex == INT_MIN)
                        {
                            minFIndex = static_cast<int>(*it);
                            f_index = it - frames.begin();
                        }
                    }
                }
                else
                    f_index = (frames.end() - frames.begin()) - 1;
            }
            return f_index;
        }
    
        std::string getPath()
        {
            char* aton_path = getenv("ATON_CAPTURE_PATH");
            
            // Get OS specific tmp directory path
            std::string def_path = boost::filesystem::temp_directory_path().string();

            if (aton_path != NULL)
                def_path = aton_path;
            
            boost::replace_all(def_path, "\\", "/");

            return def_path;
        }
    
        int getPort()
        {
            char* aton_port = getenv("ATON_PORT");
            int def_port = 9201;
            
            if (aton_port != NULL)
                def_port = atoi(aton_port);
            
            return def_port;
        }

        std::string getDateTime()
        {
            // Returns date and time
            time_t rawtime;
            struct tm* timeinfo;
            char time_buffer[20];

            time (&rawtime);
            timeinfo = localtime(&rawtime);

            // Setting up the Date and Time format style
            strftime(time_buffer, 20, "%Y-%m-%d_%H-%M-%S", timeinfo);

            return std::string(time_buffer);
        }

        std::vector<std::string> getCaptures()
        {
            // Our captured filenames list
            std::vector<std::string> results;
            
            // If the directory exist
            if (isPathValid(m_path))
            {
                using namespace boost::filesystem;
                path filepath(m_path);
                directory_iterator it(filepath.parent_path());
                directory_iterator end;

                // Regex expression to find captured files
                std::string exp = ( boost::format("%s.+.%s")%filepath.stem().string()
                                                            %filepath.extension().string() ).str();
                const boost::regex filter(exp);

                // Iterating through directory to find matching files
                BOOST_FOREACH(path const &p, std::make_pair(it, end))
                {
                    if(is_regular_file(p))
                    {
                        boost::match_results<std::string::const_iterator> what;
                        if (boost::regex_search(it->path().filename().string(),
                                                what, filter, boost::match_default))
                        {
                            std::string res = p.filename().string();
                            results.push_back(res);
                        }
                    }
                }
            }
            return results;
        }

        void cleanByLimit()
        {
            if ( !m_garbageList.empty() )
            {
                // In windows sometimes files can't be deleted due to lack of
                // access so we collecting a garbage list and trying to remove
                // them next time when user make a capture
                std::vector<std::string>::iterator it;
                for(it = m_garbageList.begin(); it != m_garbageList.end(); ++it)
                    std::remove(it->c_str());
            }

            std::vector<std::string> captures = getCaptures();
            
            using namespace boost::filesystem;
            path filepath(m_path);
            path dir = filepath.parent_path();

            // Reverse iterating through file list
            if ( !captures.empty() )
            {
                std::vector<std::string>::reverse_iterator it;
                for(it = captures.rbegin(); it != captures.rend(); ++it)
                {
                    path file = *it;
                    path path = dir / file;
                    std::string str_path = path.string();
                    boost::replace_all(str_path, "\\", "/");

                    // Remove the file if it's out of limit
                    if ((it - captures.rbegin()) >= m_slimit)
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
    
        void clearAllCmd()
        {
            std::vector<FrameBuffer>& fBs  = m_node->m_framebuffers;
            std::vector<double>& frames  = m_node->m_frames;
        
            if (!fBs.empty() && !frames.empty())
            {
                std::vector<FrameBuffer>::iterator it;
                for(it = fBs.begin(); it != fBs.end(); ++it)
                    it->ready(false);
                
                m_node->m_legit = false;
                m_node->disconnect();
            
                fBs =  std::vector<FrameBuffer>();
                frames = std::vector<double>();
                resetChannels(m_node->m_channels);
               
                m_node->m_legit = true;
                flagForUpdate();
                setStatus();
            }
        }

        void captureCmd()
        {
            std::string path = std::string(m_path);

            if (m_node->m_frames.size() > 0 && isPathValid(path) && m_slimit > 0)
            {
                // Add date or frame suffix to the path
                std::string key (".");
                std::string timeFrameSuffix;
                std::string frames;
                double startFrame;
                double endFrame;
                
                std::vector<double> sortedFrames = m_node->m_frames;
                std::stable_sort(sortedFrames.begin(), sortedFrames.end());

                if (m_multiframes && m_all_frames)
                {
                    timeFrameSuffix += "_" + std::string("####");
                    startFrame = sortedFrames.front();
                    endFrame = sortedFrames.back();
                    
                    std::vector<double>::iterator it;
                    for(it = sortedFrames.begin(); it != sortedFrames.end(); ++it)
                        frames += (boost::format("%s,")%*it).str();
                    
                    frames.resize(frames.size() - 1);
                }
                else
                {
                    timeFrameSuffix += "_" + getDateTime();
                    startFrame = endFrame = uiContext().frame();
                    frames = (boost::format("%s")%uiContext().frame()).str();
                }

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
                    double fontSize = m_stamp_scale * 0.12;
                
                    // Add text node in between to put a stamp on the capture
                    cmd = (boost::format("stamp = nuke.nodes.Text2();"
                                         "stamp['message'].setValue('''[python {nuke.toNode('%s')['status_knob'].value()}] | Comment: %s''');"
                                         "stamp['global_font_scale'].setValue(%s);"
                                         "stamp['yjustify'].setValue('bottom');"
                                         "stamp['color'].setValue(0.5);"
                                         "stamp['enable_background'].setValue(True);"
                                         "stamp['background_color'].setValue([0.05, 0.05, 0.05, 1]);"
                                         "stamp['background_opacity'].setValue(0.9);"
                                         "stamp['background_border_x'].setValue(10000);"
                                         "stamp.setInput(0, nuke.toNode('%s'));"
                                         "nuke.toNode('%s').setInput(0, stamp)")%m_node->m_node_name
                                                                                %m_comment
                                                                                %fontSize
                                                                                %m_node->m_node_name
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
                                                                                      %frames).str();
                script_command(cmd.c_str(), true, false);
                script_unlock();
            }
            cleanByLimit();
        }

        void importLatestCmd()
        {
            std::vector<std::string> captures = getCaptures();
            
            using namespace boost::filesystem;
            path filepath(m_path);
            path dir = filepath.parent_path();

            if (!captures.empty() )
            {
                // Getting last ellemnt of the vector
                path file = captures.back();
                path path = dir / file;
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
            
            using namespace boost::filesystem;
            path filepath(m_path);
            path dir = filepath.parent_path();

            if (!captures.empty())
            {
                // Reverse iterating through vector
                for(std::vector<std::string>::reverse_iterator it = captures.rbegin();
                                                               it != captures.rend(); ++it)
                {
                    path file = *it;
                    path path = dir / file;
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
    
        void setStatus(const long long& progress = 0,
                       const long long& ram = 0,
                       const long long& p_ram = 0,
                       const int& time = 0,
                       const double& frame = 0,
                       const std::string& version = "")
        {
            int hour = time / 3600000;
            int minute = (time % 3600000) / 60000;
            int second = ((time % 3600000) % 60000) / 1000;
            size_t f_count = m_node->m_framebuffers.size();

            std::string str_status = (boost::format("Arnold: %s | "
                                                    "Memory: %sMB / %sMB | "
                                                    "Time: %02ih:%02im:%02is | "
                                                    "Frame: %04i (%s) | "
                                                    "Progress: %s%%")%version%ram%p_ram
                                                                     %hour%minute%second
                                                                     %frame%f_count%progress).str();
            knob("status_knob")->set_text(str_status.c_str());
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
    using namespace boost;
    Aton* node = reinterpret_cast<Aton*>(data);
    std::vector<FrameBuffer>& fBs = node->m_framebuffers;
    
    double uiFrame = 0;
    double prevFrame = 0;
    int milliseconds = 10;

    while (node->m_legit)
    {
        uiFrame = node->uiContext().frame();
        if (!fBs.empty() && prevFrame != uiFrame)
        {
            node->flagForUpdate();
            prevFrame = uiFrame;
        }
        this_thread::sleep(posix_time::millisec(milliseconds));
    }
}

// Listening thread method
static void atonListen(unsigned index, unsigned nthreads, void* data)
{
    bool killThread = false;
    std::vector<std::string> active_aovs;

    Aton* node = reinterpret_cast<Aton*> (data);

    while (!killThread)
    {
        // Accept incoming connections!
        node->m_server.accept();

        // Our incoming data object
        Data d;

        // For progress percentage
        long long progress = 0;
        long long _regionArea = 0;
        
        long f_index = 0;
        
        // Current Frame Number
        double current_frame = 0;
        
        // For time to reset per every IPR iteration
        static int active_time = 0;
        static int delta_time = 0;

        // Loop over incoming data
        while ((d.type() == 2 || d.type() == 9) == false)
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
                    // Copy data from d
                    int _width = d.width();
                    int _height = d.height();
                    double _frame = static_cast<double>(d.currentFrame());
                    
                    if (current_frame != _frame)
                        current_frame = _frame;
                    
                    // Create FrameBuffer
                    if (std::find(node->m_frames.begin(),
                                  node->m_frames.end(),
                                  _frame) == node->m_frames.end())
                    {
                        FrameBuffer fB(_frame, _width, _height);
                        if (!node->m_frames.empty())
                            fB = node->m_framebuffers.back();
                        node->m_mutex.lock();
                        node->m_frames.push_back(_frame);
                        node->m_framebuffers.push_back(fB);
                        node->m_mutex.unlock();
                    }
                    if (!node->m_multiframes)
                    {
                        node->m_frames.resize(1);
                        node->m_framebuffers.resize(1);
                    }
                    
                    // Get current FrameBuffer
                    f_index = node->getFrameIndex(_frame);
                    FrameBuffer& fB = node->m_framebuffers[f_index];
                    
                    // Reset Buffers and Channels if needed
                    if (!active_aovs.empty() && !fB.empty())
                    {
                        switch (fB.compare(_width,
                                           _height,
                                           _frame, active_aovs))
                        {
                            case 1: // Frame mismatch
                            {
                                node->m_mutex.lock();
                                fB.setFrame(_frame);
                                break;
                            }
                            
                            case 2: // Frame and AOVs mismatch
                            {
                                node->m_mutex.lock();
                                fB.resize(1);
                                fB.ready(false);
                                fB.setFrame(_frame);
                                node->resetChannels(node->m_channels);
                                break;
                            }
                            case 3: // All mismatch
                            {
                                node->m_mutex.lock();
                                fB.clearAll();
                                fB.setWidth(_width);
                                fB.setHeight(_height);
                                fB.ready(false);
                                node->resetChannels(node->m_channels);
                                break;
                            }
                        }
                        node->m_mutex.unlock();
                    }
                    
                    // Get image area to calculate the progress
                    _regionArea = d.rArea();
                    
                    // Get delta time per IPR iteration
                    delta_time = active_time;
                    
                    // Set Arnold Core version
                    fB.setArnoldVersion(d.version());
                    
                    // Set time to current frame
                    if (node->m_multiframes &&
                        node->uiContext().frame() != current_frame)
                        node->setCurrentFrame(current_frame);
                    
                    // Reset active AOVs
                    if(!active_aovs.empty())
                        active_aovs.clear();
                    break;
                }
                case 1: // image data
                {
                    // Get frame buffer
                    FrameBuffer& fB = node->m_framebuffers[f_index];
                    const char* _aov_name = d.aovName();
                    
                    // Get active time
                    active_time = d.time();

                    // Get active aov names
                    if(std::find(active_aovs.begin(),
                                 active_aovs.end(),
                                 _aov_name) == active_aovs.end())
                    {
                        if (node->m_enable_aovs)
                            active_aovs.push_back(_aov_name);
                        else if (active_aovs.size() == 0)
                            active_aovs.push_back(_aov_name);
                        else if (active_aovs.size() > 1)
                            active_aovs.resize(1);
                    }
                    
                    // Skip non RGBA buckets if AOVs are disabled
                    if (node->m_enable_aovs || active_aovs[0] == _aov_name)
                    {
                        // Copy data from d
                        int _x = d.x();
                        int _y = d.y();
                        int _width = d.width();
                        int _height = d.height();
                        int _spp = d.spp();
                        long long _ram = d.ram();
                        int _time = d.time();
                        const float* _pix_data = d.pixels();
                        
                        const int& w = fB.getWidth();
                        const int& h = fB.getHeight();
                        unsigned int x, y, c, offset;
                    
                        // Lock buffer
                        node->m_mutex.lock();
                        
                        // Adding buffer
                        if(!fB.bufferNameExists(_aov_name) &&
                           (node->m_enable_aovs || fB.size() == 0))
                            fB.addBuffer(_aov_name, _spp);
                        else
                            fB.ready(true);
                        
                        // Get buffer index
                        long b = fB.getBufferIndex(_aov_name);
                       
                         // Writing to buffer
                        for (x = 0; x < _width; ++x)
                        {
                            for (y = 0; y < _height; ++y)
                            {
                                offset = (_width * y * _spp) + (x * _spp);
                                for (c = 0; c < _spp; ++c)
                                {
                                    float& p = fB.setBufferPix(b, x + _x,
                                                               h - (y + _y + 1),
                                                               _spp, c);
                                    p = _pix_data[offset + c];
                                }
                            }
                        }

                        // Release lock
                        node->m_mutex.unlock();

                        // Update only on first aov
                        if(!node->m_capturing &&
                           fB.getFirstBufferName().compare(_aov_name) == 0)
                        {
                            // Calculate the progress percentage
                            _regionArea -= (_width*_height);
                            progress = 100 - (_regionArea * 100) / (w * h);
                           
                             // Set bucket size
                            node->m_mutex.lock();
                            fB.setBucketBBox(_x, h - _y - _height, _x + _width,
                                             h - _y);
                            
                            // Set status parameters
                            fB.setProgress(progress);
                            fB.setRAM(_ram);
                            if (delta_time > _time)
                                fB.setTime(_time);
                            else
                                fB.setTime(_time - delta_time);
                            node->m_mutex.unlock();
                            
                            // Update the image
                            node->flagForUpdate(f_index);
                        }
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
