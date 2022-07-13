// RTSP Server

#include <thread>
#include <memory>
#include <iostream>
#include <string>
#include "xop/RtspServer.h"
#include "net/Timer.h"
#include "bs.h"
#include "h265_stream.h"

static volatile bool    s_spnet_loop;
static pthread_mutex_t  s_main_lock;
static pthread_cond_t   s_frame_cond;

class H265File
{
public:
	H265File(int buf_size=500000);
	~H265File();

	bool Open(const char *path);
	void Close();

	bool IsOpened() const
	{ return (m_file != NULL); }

	int ReadFrame(char* in_buf, int in_buf_size, bool* end);
    
private:
    FILE*       m_file          = NULL;
    uint8_t*    m_buf           = NULL;
    int         m_buf_size      = 0;
    int         m_bytes_used    = 0;
    int         m_count         = 0;
};

H265File::H265File(int buf_size)
: m_buf_size(buf_size)
{
    m_buf = new uint8_t[m_buf_size];
}

H265File::~H265File()
{
    delete [] m_buf;
}

bool H265File::Open(const char *path)
{
    m_file = fopen(path, "rb");
    if(m_file == NULL) {
        return false;
    }

    return true;
}

void H265File::Close()
{
    if(m_file) {
        fclose(m_file);
        m_file = NULL;
        m_count = 0;
        m_bytes_used = 0;
    }
}

int H265File::ReadFrame(char* in_buf, int in_buf_size, bool* end)
{
    if(m_file == NULL) {
        return -1;
    }

    int bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
    if(bytes_read == 0) {
        fseek(m_file, 0, SEEK_SET);
        m_count = 0;
        m_bytes_used = 0;
        bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
        if(bytes_read == 0)         {
            this->Close();
            return -1;
        }
    }

    uint8_t     nal_unit;
    uint32_t    first_slice,no_output,pic_parameter,slice_type;
    bool is_find_start = false, is_find_end = false;
    int i = 0, start_code = 3;
    *end = false;

    for (i=0; i<bytes_read-5; i++) {
        if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1) {
            start_code = 3;
        }
        else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1) {
            start_code = 4;
        }
        else  {
            continue;
        }

        /*
        00 00 00 01 nalu temporal_id
        0   CODED SLICE TRAIL N
        1   CODED SLICE TRAIL R
        0x13 IDR W
        0x14 IDR N
        0x20 VPS
        0x21 SPS
        0x22 PPS
        0x27 SEI Prefix
        0x28 SEI Suffix
        nal_unit    = (*nalu_pos&0x7E) >> 1
        temporal_id = *(nalu_pos+1)
        --------------------------------
        00 00 00 01 nalu temporal_id (first_slice+no_output+slice_pic_parameter+slice_reserved+slice_type at one byte)
        0   HEVC_SLICE_B
        1   HEVC_SLICE_P
        2   HEVC_SLICE_I
        *(bs->p)    = *(nalu_pos+2)
        bs_read_u1(bs)
        bs_read_u1(bs)
        bs_read_ue(bs)
        slice_type  = bs_read_ue(bs)
        */

        //debug
        /*printf("ReadFrame1: %02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n",
                    *((unsigned char*)(m_buf+0)),
                    *((unsigned char*)(m_buf+1)),
                    *((unsigned char*)(m_buf+2)),
                    *((unsigned char*)(m_buf+3)),
                    *((unsigned char*)(m_buf+4)),
                    *((unsigned char*)(m_buf+5)),
                    *((unsigned char*)(m_buf+6)),
                    *((unsigned char*)(m_buf+7)),
                    *((unsigned char*)(m_buf+8)),
                    *((unsigned char*)(m_buf+9)),
                    *((unsigned char*)(m_buf+10)),
                    *((unsigned char*)(m_buf+11)));

        if( *((unsigned char*)(m_buf+0)) == 0x00
                    &&
            *((unsigned char*)(m_buf+1)) == 0x00
                    &&
            *((unsigned char*)(m_buf+2)) == 0x00
                    &&
            *((unsigned char*)(m_buf+3)) == 0x01
                    &&
            *((unsigned char*)(m_buf+4)) == 0x10
                    &&
            *((unsigned char*)(m_buf+5)) == 0x01
                    &&
            *((unsigned char*)(m_buf+6)) == 0xff
                    &&
            *((unsigned char*)(m_buf+7)) == 0x22
                    &&
            *((unsigned char*)(m_buf+8)) == 0x2d
                    &&
            *((unsigned char*)(m_buf+9)) == 0x49
                    &&
            *((unsigned char*)(m_buf+10)) == 0xfd
                    &&
            *((unsigned char*)(m_buf+11)) == 0xc2)
                printf("found1\n\n");*/

        nal_unit = (m_buf[i+start_code] & 0x7E) >> 1;

        bs_init(m_buf[i+start_code+2]);
        first_slice     = bs_read_u1();
        if ( nal_unit >= NAL_UNIT_CODED_SLICE_BLA_W_LP && nal_unit <= NAL_UNIT_RESERVED_IRAP_VCL23 )
        {
            no_output = bs_read_u1();
        }
        pic_parameter   = bs_read_ue();
        slice_type      = bs_read_ue();

        /*SLICE_TRAIL_N SLICE_TRAIL_R IDRW IDRN CRA && HEVC_SLICE_B HEVC_SLICE_I*/
        if (nal_unit == 0x20 || nal_unit == 0x21 || nal_unit == 0x22 || nal_unit == 0x27 || nal_unit == 0x28
            ||
            ((nal_unit == 0x0 || nal_unit == 0x1 || nal_unit == 0x8 || nal_unit == 0x9 || nal_unit == 0x13 || nal_unit == 0x14 || nal_unit == 0x15) && (slice_type >= 0 && slice_type <= 2))) {
            //printf("slice_type:%d\n", slice_type); //debug
            is_find_start = true;
            i += 4;
            break;
        }
    }

    for (; i<bytes_read-5; i++) {
        if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1)
        {
            start_code = 3;
        }
        else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1) {
            start_code = 4;
        }
        else   {
            continue;
        }

        //debug
        /*printf("ReadFrame2: %02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n",
                *((unsigned char*)(i+m_buf+0)),
                *((unsigned char*)(i+m_buf+1)),
                *((unsigned char*)(i+m_buf+2)),
                *((unsigned char*)(i+m_buf+3)),
                *((unsigned char*)(i+m_buf+4)),
                *((unsigned char*)(i+m_buf+5)),
                *((unsigned char*)(i+m_buf+6)),
                *((unsigned char*)(i+m_buf+7)),
                *((unsigned char*)(i+m_buf+8)),
                *((unsigned char*)(i+m_buf+9)),
                *((unsigned char*)(i+m_buf+10)),
                *((unsigned char*)(i+m_buf+11)));

        if( *((unsigned char*)(i+m_buf+0)) == 0x00
                    &&
            *((unsigned char*)(i+m_buf+1)) == 0x00
                    &&
            *((unsigned char*)(i+m_buf+2)) == 0x00
                    &&
            *((unsigned char*)(i+m_buf+3)) == 0x01
                    &&
            *((unsigned char*)(i+m_buf+4)) == 0x00
                    &&
            *((unsigned char*)(i+m_buf+5)) == 0x01
                    &&
            *((unsigned char*)(i+m_buf+6)) == 0xfe
                    &&
            *((unsigned char*)(i+m_buf+7)) == 0xe2
                    &&
            *((unsigned char*)(i+m_buf+8)) == 0x2d
                    &&
            *((unsigned char*)(i+m_buf+9)) == 0x57
                    &&
            *((unsigned char*)(i+m_buf+10)) == 0xf7
                    &&
            *((unsigned char*)(i+m_buf+11)) == 0x08)
            printf("found2\n\n");*/

        nal_unit = (m_buf[i+start_code] & 0x7E) >> 1;

        bs_init(m_buf[i+start_code+2]);
        first_slice     = bs_read_u1();
        if ( nal_unit >= NAL_UNIT_CODED_SLICE_BLA_W_LP && nal_unit <= NAL_UNIT_RESERVED_IRAP_VCL23 )
        {
            no_output = bs_read_u1();
        }
        pic_parameter   = bs_read_ue();
        slice_type      = bs_read_ue();

        /*VPS SPS PPS SEI_PREFIX SEI_SUFFIX
          SLICE_TRAIL_N SLICE_TRAIL_R IDRW IDRN CRA && HEVC_SLICE_B HEVC_SLICE_I*/
        if (nal_unit == 0x20 || nal_unit == 0x21 || nal_unit == 0x22 || nal_unit == 0x27 || nal_unit == 0x28
            ||
            ((nal_unit == 0x0 || nal_unit == 0x1 || nal_unit == 0x8 || nal_unit == 0x9 || nal_unit == 0x13 || nal_unit == 0x14 || nal_unit == 0x15) && (slice_type >= 0 && slice_type <= 2))) {
            //printf("slice_type:%d\n", slice_type); //debug
            is_find_end = true;
            break;
        }
    }

    bool flag = false;
    if(is_find_start && !is_find_end && m_count>0) {
        flag = is_find_end = true;
        i = bytes_read;
        *end = true;
    }

    if(!is_find_start || !is_find_end) {
        this->Close();
        return -1;
    }

    int size = (i<=in_buf_size ? i : in_buf_size);
    memcpy(in_buf, m_buf, size);

    if(!flag) {
        m_count += 1;
        m_bytes_used += i;
    }
    else {
        m_count = 0;
        m_bytes_used = 0;
    }

    fseek(m_file, m_bytes_used, SEEK_SET);
    return size;
}

static void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H265File* h265_file)
{
    int buf_size = 2000000;
    std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

    while(s_spnet_loop) {
        bool end_of_frame = false;
        int frame_size = h265_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
        if(frame_size > 0) {
            xop::AVFrame videoFrame = {0};
            //videoFrame.type = 0;
            videoFrame.size = frame_size;
            //videoFrame.timestamp = xop::H265Source::GetTimestamp();
            xop::H265Source::GetTimestamp(&videoFrame.timeNow, &videoFrame.timestamp);
            videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
            memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
            rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);
        }
        else {
            break;
        }

        xop::Timer::Sleep(40);
    }

    pthread_cond_signal(&s_frame_cond);
}

int main(int argc, char **argv)
{	
	if(argc != 2) {
		printf("Usage: %s test.265\n", argv[0]);
		return 0;
	}

	H265File h265_file;
	if(!h265_file.Open(argv[1])) {
		printf("Open %s failed.\n", argv[1]);
		return 0;
	}

	pthread_mutex_init(&s_main_lock,NULL);
	pthread_cond_init(&s_frame_cond,NULL);

	std::string suffix = "live";
	std::string ip = "127.0.0.1";
	std::string port = "554";
	std::string rtsp_url = "rtsp://" + ip + ":" + port + "/" + suffix;
	
	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());

	if (!server->Start("0.0.0.0", atoi(port.c_str()))) {
		printf("RTSP Server listen on %s failed.\n", port.c_str());
		return 0;
	}

#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif

	xop::MediaSession *session = xop::MediaSession::CreateNew("live"); 
	session->AddSource(xop::channel_0, xop::H265Source::CreateNew());
	//session->StartMulticast(); 
	session->AddNotifyConnectedCallback([] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});
   
	session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});

	xop::MediaSessionId session_id = server->AddSession(session);
         
	s_spnet_loop = true;
	std::thread t1(SendFrameThread, server.get(), session_id, &h265_file);
	t1.detach();

	std::cout << "Play URL: " << rtsp_url << std::endl;

	/*while (1) {
		xop::Timer::Sleep(100);
	}*/
	getchar();

	s_spnet_loop = false;
    pthread_mutex_lock(&s_main_lock);
	pthread_cond_wait(&s_frame_cond, &s_main_lock);
	pthread_mutex_unlock(&s_main_lock);

    pthread_cond_destroy(&s_frame_cond);
    pthread_mutex_destroy(&s_main_lock);

    h265_file.Close();

    std::cout << "rtsp server exit" << std::endl;

	return 0;
}
