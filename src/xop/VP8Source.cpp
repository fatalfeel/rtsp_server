
#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "VP8Source.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__)
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

VP8Source::VP8Source(uint32_t framerate)
    : framerate_(framerate)
{
    payload_    = 96;
    clock_rate_ = 90000;
}

VP8Source* VP8Source::CreateNew(uint32_t framerate)
{
    return new VP8Source(framerate);
}

VP8Source::~VP8Source()
{

}

string VP8Source::GetMediaDescription(uint16_t port)
{
    char buf[100] = { 0 };
    sprintf(buf, "m=video %hu RTP/AVP 96", port);
    return string(buf);
}

string VP8Source::GetAttribute()
{
    return string("a=rtpmap:96 VP8/90000");
}

bool VP8Source::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t* frame_buf = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.timestamp == 0) {
        //frame.timestamp = GetTimestamp();
        GetTimestamp(&frame.timeNow, &frame.timestamp);
    }

    // X = R = N = 0; PartID = 0; 
    // S = 1 if this is the first (or only) fragment of the frame
    uint8_t vp8_payload_descriptor = 0x10;

    while (frame_size > 0) {
        uint32_t payload_size = MAX_RTP_PAYLOAD_SIZE;

        RtpPacket rtp_pkt;

        //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
        rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
        //rtp_pkt.type = frame.type;
        rtp_pkt.timestamp   = frame.timestamp;
        rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + RTP_VPX_HEAD_SIZE + MAX_RTP_PAYLOAD_SIZE;
        rtp_pkt.last        = 0;

        if (frame_size < MAX_RTP_PAYLOAD_SIZE) {
            payload_size = frame_size;
            rtp_pkt.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + RTP_VPX_HEAD_SIZE + frame_size;
            rtp_pkt.last = 1;
        }

        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = vp8_payload_descriptor;
        memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + RTP_VPX_HEAD_SIZE, frame_buf, payload_size);

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

        frame_buf  += payload_size;
        frame_size -= payload_size;
        vp8_payload_descriptor = 0x00;
    }

    return true;
}

void VP8Source::GetTimestamp(struct timeval* tv, uint32_t* ts)
{
    //auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    //return (uint32_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90);

    uint32_t timestamp;
    gettimeofday(tv, NULL);
    //*ts = ((tv->tv_sec*1000)+((tv->tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    timestamp   = (tv->tv_sec + tv->tv_usec/1000000.0)*90000 + 0.5; //90khz sound
    *ts         = htonl(timestamp);
}
