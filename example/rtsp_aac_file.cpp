// RTSP Server

#include "xop/RtspServer.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

static volatile bool    s_spnet_loop;
static pthread_mutex_t  s_main_lock;
static pthread_cond_t   s_frame_cond;

class AACFile
{
public:
	AACFile(int buf_size=500000);
	~AACFile();

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

AACFile::AACFile(int buf_size)
: m_buf_size(buf_size)
{
    m_buf = new uint8_t[m_buf_size];
}

AACFile::~AACFile()
{
    delete [] m_buf;
}

bool AACFile::Open(const char *path)
{
    m_file = fopen(path, "rb");
    if(m_file == NULL) {
        return false;
    }

    return true;
}

void AACFile::Close()
{
    if(m_file) {
        fclose(m_file);
        m_file = NULL;
        m_count = 0;
        m_bytes_used = 0;
    }
}

int AACFile::ReadFrame(char* in_buf, int in_buf_size, bool* end)
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

    bool is_find_start = false, is_find_end = false;
    int i = 0;
    *end = false;

    for (i=0; i<bytes_read; i++) {
        if(m_buf[i] == 0xff && (m_buf[i+1] & 0xf6)== 0xf0) {
            is_find_start = true;
            break;
        }
        else  {
            continue;
        }
    }

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
                *((unsigned char*)(m_buf+11)));*/

    /*if( *((unsigned char*)(m_buf+0)) == 0xff
                    &&
            *((unsigned char*)(m_buf+1)) == 0xf1
                    &&
            *((unsigned char*)(m_buf+2)) == 0x50
                    &&
            *((unsigned char*)(m_buf+3)) == 0x80
                    &&
            *((unsigned char*)(m_buf+4)) == 0xbb
                    &&
            *((unsigned char*)(m_buf+5)) == 0xff
                    &&
            *((unsigned char*)(m_buf+6)) == 0xfc
                    &&
            *((unsigned char*)(m_buf+7)) == 0x21
                    &&
            *((unsigned char*)(m_buf+8)) == 0x4c
                    &&
            *((unsigned char*)(m_buf+9)) == 0x6c
                    &&
            *((unsigned char*)(m_buf+10)) == 0xf8
                    &&
            *((unsigned char*)(m_buf+11)) == 0x17)
            printf("found1\n\n");*/

    uint32_t frame_length = (((unsigned int)m_buf[3] & 0x3) << 11)
                            |
                            (((unsigned int)m_buf[4]) << 3)
                            |
                            (m_buf[5] >> 5);

    i += frame_length;
    is_find_end = true;

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
            *((unsigned char*)(i+m_buf+11)));*/

    /*if( *((unsigned char*)(i+m_buf+0)) == 0xff
                &&
        *((unsigned char*)(i+m_buf+1)) == 0xf1
                &&
        *((unsigned char*)(i+m_buf+2)) == 0x50
                &&
        *((unsigned char*)(i+m_buf+3)) == 0x80
                &&
        *((unsigned char*)(i+m_buf+4)) == 0xbb
                &&
        *((unsigned char*)(i+m_buf+5)) == 0xff
                &&
        *((unsigned char*)(i+m_buf+6)) == 0xfc
                &&
        *((unsigned char*)(i+m_buf+7)) == 0x21
                &&
        *((unsigned char*)(i+m_buf+8)) == 0x4c
                &&
        *((unsigned char*)(i+m_buf+9)) == 0x6c
                &&
        *((unsigned char*)(i+m_buf+10)) == 0xf8
                &&
        *((unsigned char*)(i+m_buf+11)) == 0x17)
        printf("found2\n\n");*/


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

static void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, AACFile* aac_file)
{
    int buf_size = 2000000;
    std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

    while(s_spnet_loop) {
        bool end_of_frame = false;
        int frame_size = aac_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
        if(frame_size > 0) {
            xop::AVFrame audioFrame = {0};
            //audioFrame.type = 0;
            audioFrame.size = frame_size;
            //audioFrame.timestamp = xop::AACSource::GetTimestamp();
            xop::AACSource::GetTimestamp(&audioFrame.timeNow, &audioFrame.timestamp);
            audioFrame.buffer.reset(new uint8_t[audioFrame.size]);
            memcpy(audioFrame.buffer.get(), frame_buf.get(), audioFrame.size);
            rtsp_server->PushFrame(session_id, xop::channel_0, audioFrame);
        }
        else {
            break;
        }

        xop::Timer::Sleep(18);
    }

    pthread_cond_signal(&s_frame_cond);
}

int main(int argc, char **argv)
{	
	if(argc != 2) {
		printf("Usage: %s test.aac\n", argv[0]);
		return 0;
	}

	AACFile aac_file;
	if(!aac_file.Open(argv[1])) {
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
	session->AddSource(xop::channel_0, xop::AACSource::CreateNew());
	//session->StartMulticast(); 
	session->AddNotifyConnectedCallback([] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});
   
	session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});

	xop::MediaSessionId session_id = server->AddSession(session);
         
	s_spnet_loop = true;
	std::thread t1(SendFrameThread, server.get(), session_id, &aac_file);
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

    aac_file.Close();

    std::cout << "rtsp server exit" << std::endl;

	return 0;
}
