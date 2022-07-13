// PHZ
// 2018-5-16

#if defined(WIN32) || defined(_WIN32) 
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif
#include "G711ASource.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__) 
#include <sys/time.h>
#endif 

using namespace xop;
using namespace std;

G711ASource::G711ASource()
{
	payload_    = 8;
	media_type_ = PCMA;
	clock_rate_ = 8000;
}

G711ASource* G711ASource::CreateNew()
{
    return new G711ASource();
}

G711ASource::~G711ASource()
{
	
}

string G711ASource::GetMediaDescription(uint16_t port)
{
	char buf[100] = {0};
	sprintf(buf, "m=audio %hu RTP/AVP 8", port);
	return string(buf);
}
	
string G711ASource::GetAttribute()
{
    return string("a=rtpmap:8 PCMA/8000/1");
}

bool G711ASource::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t *frame_buf  = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.size > MAX_RTP_PAYLOAD_SIZE) {
		return false;
	}

	if (frame.timestamp == 0) {
        GetTimestamp(&frame.timeNow, &frame.timestamp);
	}

	RtpPacket rtp_pkt;

	//rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
	rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
	//rtp_pkt.type        = frame.type;
	rtp_pkt.timestamp   = frame.timestamp;
	rtp_pkt.timeNow     = frame.timeNow;
	rtp_pkt.size        = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
	rtp_pkt.last        = 1;

	memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE, frame_buf, frame_size);

	/*if (send_frame_callback_)
	{
		if (!send_frame_callback_(channel_id, rtp_pkt))
        {
            return false;
        }
	}*/
    if (m_pThis && m_pCallMember)
    {
        (m_pThis->*m_pCallMember)(channel_id, rtp_pkt);
    }

	rtp_pkt.data.reset();

	return true;
}

void G711ASource::GetTimestamp(struct timeval* tv, uint32_t* ts)
{
    uint32_t timestamp;
    gettimeofday(tv, NULL);
    //*ts = ((tv->tv_sec*1000)+((tv->tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    timestamp   = (tv->tv_sec + tv->tv_usec/1000000.0)*8000 + 0.5; //8khz sound
    *ts         = htonl(timestamp);

    /*auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
	return (uint32_t)((time_point.time_since_epoch().count()+500)/1000*8);*/
}

