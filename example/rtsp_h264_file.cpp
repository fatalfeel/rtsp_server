// RTSP Server

#include <thread>
#include <memory>
#include <iostream>
#include <string>
#include "xop/RtspServer.h"
#include "net/Timer.h"
#include "bs.h"

static volatile bool    s_spnet_loop;
static pthread_mutex_t  s_main_lock;
static pthread_cond_t   s_frame_cond;

class H264File
{
public:
	H264File(int buf_size=500000);
	~H264File();

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

H264File::H264File(int buf_size)
: m_buf_size(buf_size)
{
    m_buf = new uint8_t[m_buf_size];
}

H264File::~H264File()
{
    delete [] m_buf;
}

bool H264File::Open(const char *path)
{
    m_file = fopen(path, "rb");
    if(m_file == NULL) {
        return false;
    }

    return true;
}

void H264File::Close()
{
    if(m_file) {
        fclose(m_file);
        m_file = NULL;
        m_count = 0;
        m_bytes_used = 0;
    }
}

int H264File::ReadFrame(char* in_buf, int in_buf_size, bool* end)
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
    uint32_t    first_mb, slice_type;
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
        nal unit = *pdata & 0x1F
        FU indicator format same as NALU format
        |7|6|5|4|3|2|1|0|
        |F|NRI|  Type   |
        F    : forbidden zero bit, normally is 0.
               In some cases, if the NALU loses data, this bit can be set to 1
               so that the receiver can correct the error or drop the unit.
        NRI  : nal_ref_idc, Indicates the importance of the data.
        Type : nal_unit_type, 1~23

        enum H264NALTYPE
        {
            H264NT_NAL = 0,
            H264NT_SLICE,
            H264NT_SLICE_DPA,
            H264NT_SLICE_DPB,
            H264NT_SLICE_DPC,
            H264NT_SLICE_IDR,
            H264NT_SEI,
            H264NT_SPS,
            H264NT_PPS
        };
        ----------------------------------------------------------
        00 00 00 01 nalu profile_idc
        profile_idc = *(nalu_pos+1)
        0x2C CAVLC 4:4:4 Intra profile
        0x42 Baseline profile
        0x4D main profile
        0x58 extended profile
        0x64 High Profile
        0x6E High 10 profile
        0x7A High 4:2:2 profile
        0xF4 High 4:4:4 Predictive profile
        ----------------------------------------------------------
        https://github.com/Gosivn/Exp-Golomb-C-implementation
        00 00 01 nalu (first_mb+slice_type at one byte)
        *(bs->p) = *(nalu_pos+1)
        first_mb    bs_read_ue(bs)
        slice_type  bs_read_ue(bs)
        0x40        first_mb=0x01, slice_type=0x00 (P  slice)
        0x42        first_mb=0x01, slice_type=0x07 (I  slice)
        0x44        first_mb=0x01, slice_type=0x03 (SP slice)
        0x45        first_mb=0x01, slice_type=0x04 (SI slice)
        0x46        first_mb=0x01, slice_type=0x05 (P  slice)
        0x47        first_mb=0x01, slice_type=0x06 (B  slice)
        0x48~0x4b   first_mb=0x01, slice_type=0x01 (B  slice)
        0x4c~0x4f   first_mb=0x01, slice_type=0x02 (I  slice)
        0x80        first_mb=0x00, slice_type=0x00 (P  slice)
        0x88        first_mb=0x00, slice_type=0x07 (I  slice)
        0x89        first_mb=0x00, slice_type=0x08 (SP slice)
        0x8a        first_mb=0x00, slice_type=0x09 (SI slice)
        0x90~0x93   first_mb=0x00, slice_type=0x03 (SP slice)
        0x94~0x97   first_mb=0x00, slice_type=0x04 (SI slice)
        0x98~0x9b   first_mb=0x00, slice_type=0x05 (P  slice)
        0x9c~0x9f   first_mb=0x00, slice_type=0x06 (B  slice)
        0xa0~0xaf   first_mb=0x00, slice_type=0x01 (B  slice)
        0xb0~0xbf   first_mb=0x00, slice_type=0x02 (I  slice)
        */

        //m_buf[i+start_code+1] >= 0x80 have type 0~9
        nal_unit    = m_buf[i+start_code] & 0x1F;
        bs_init(m_buf[i+start_code+1]);
        first_mb    = bs_read_ue();
        slice_type  = bs_read_ue();
        //if ((nal_unit == 0x5 || nal_unit == 0x1) && ((m_buf[i+start_code+1]&0x80) == 0x80)) {
        if (nal_unit == 0x6 || nal_unit == 0x7 || nal_unit == 0x8
            ||
            ((nal_unit == 0x1 || nal_unit == 0x5) && (slice_type >= 1 && slice_type <= 9))) {
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

        nal_unit    = m_buf[i+start_code] & 0x1F;
        bs_init(m_buf[i+start_code+1]);
        first_mb    = bs_read_ue();
        slice_type  = bs_read_ue();
        if (nal_unit == 0x6 || nal_unit == 0x7 || nal_unit == 0x8
            ||
            //((nal_unit == 0x5 || nal_unit == 0x1) && ((m_buf[i+start_code+1]&0x80) == 0x80))) {
            ((nal_unit == 0x1 || nal_unit == 0x5) && (slice_type >= 1 && slice_type <= 9))) {
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

static void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* h264_file)
{
    int buf_size = 2000000;
    std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

    while(s_spnet_loop) {
        bool end_of_frame = false;
        int frame_size = h264_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
        if(frame_size > 0) {
            xop::AVFrame videoFrame = {0};
            //videoFrame.type = 0;
            videoFrame.size = frame_size;
            //videoFrame.timestamp = xop::H264Source::GetTimestamp();
            xop::H264Source::GetTimestamp(&videoFrame.timeNow, &videoFrame.timestamp);
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
		printf("Usage: %s test.264\n", argv[0]);
		return 0;
	}

	H264File h264_file;
	if(!h264_file.Open(argv[1])) {
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
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew()); 
	//session->StartMulticast(); 
	session->AddNotifyConnectedCallback([] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});
   
	session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});

	xop::MediaSessionId session_id = server->AddSession(session);
         
	s_spnet_loop = true;
	std::thread t1(SendFrameThread, server.get(), session_id, &h264_file);
	t1.detach();

	std::cout << "Play URL: " << rtsp_url << std::endl;

	/*while (1) {
		xop::Timer::Sleep(100);
	}*/
	getchar();

	s_spnet_loop = false;
    pthread_mutex_lock(&s_main_lock);
	pthread_cond_wait(&s_frame_cond, &s_main_lock); //wait thread exit signal
	pthread_mutex_unlock(&s_main_lock);

    pthread_cond_destroy(&s_frame_cond);
    pthread_mutex_destroy(&s_main_lock);

    h264_file.Close();

    std::cout << "rtsp server exit" << std::endl;

	return 0;
}
