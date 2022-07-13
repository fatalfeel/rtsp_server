// PHZ
// 2018-5-16

#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "H264Source.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__)
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

H264Source::H264Source(uint32_t framerate)
: framerate_(framerate)
{
    payload_    = 96; 
    media_type_ = H264;
    clock_rate_ = 90000;
}

H264Source* H264Source::CreateNew(uint32_t framerate)
{
    return new H264Source(framerate);
}

H264Source::~H264Source()
{

}

string H264Source::GetMediaDescription(uint16_t port)
{
    char buf[100] = {0};
    sprintf(buf, "m=video %hu RTP/AVP 96", port); // \r\nb=AS:2000
    return string(buf);
}

string H264Source::GetAttribute()
{
    return string("a=rtpmap:96 H264/90000");
}

bool H264Source::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t* frame_buf  = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.timestamp == 0) {
        GetTimestamp(&frame.timeNow, &frame.timestamp);
    }    

    if (frame_size <= MAX_RTP_PAYLOAD_SIZE)
    {
        uint32_t start_code = 0;
        if(frame_buf[0] == 0 && frame_buf[1] == 0 && frame_buf[2] == 1)
        {
            start_code = 3;
        }
        else if(frame_buf[0] == 0 && frame_buf[1] == 0 && frame_buf[2] == 0 && frame_buf[3] == 1)
        {
            start_code = 4; //after test start_code always 4
        }
        frame_buf  += start_code; //shift to nalu position
        frame_size -= start_code;

        RtpPacket rtp_pkt;

        //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
        rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
	    //rtp_pkt.type        = frame.type;
	    rtp_pkt.timestamp   = frame.timestamp;
	    rtp_pkt.timeNow     = frame.timeNow;
	    rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + frame_size;
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
    }
    else
    {
        /*
        RTP Payload Format
        1~23   NAL unit  Single NAL unit packet per H.264
        28     FU-A      Fragmentation unit
        29     FU-B      Fragmentation unit
        ---------------------------------------------------------
        FU indicator format same as NALU format
        |7|6|5|4|3|2|1|0|
        |F|NRI|  Type   |
        F    : forbidden zero bit, normally is 0.
               In some cases, if the NALU loses data, this bit can be set to 1
               so that the receiver can correct the error or drop the unit.
        NRI  : nal_ref_idc, Indicates the importance of the data.
        Type : nal_unit_type, 1~23
        first packet    F = f of NALU, NRI = nri of NALU, type = 0x1C
        ---------------------------------------------------------
        FU header
        |7|6|5|4|3|2|1|0|
        |S|E|R|  Type   |
        first packet    S = 1，E = 0，R = 0, type is NALU type.
        middle packet   S = 0，E = 0，R = 0, type is NALU type.
        final packet    S = 0，E = 1，R = 0, type is NALU type.
        */
        uint32_t start_code = 0;
        if(frame_buf[0] == 0 && frame_buf[1] == 0 && frame_buf[2] == 1)
        {
            start_code = 3;
        }
        else if(frame_buf[0] == 0 && frame_buf[1] == 0 && frame_buf[2] == 0 && frame_buf[3] == 1)
        {
            start_code = 4; //after test start_code always 4
        }
        frame_buf  += start_code; //shift to nalu position
        frame_size -= start_code;

        char FU_A[2] = {0};
        FU_A[0] = (frame_buf[0] & 0xe0) | 28;   //FU indicator, OR 0x1C(28) make it to FU-A
        FU_A[1] = 0x80 | (frame_buf[0] & 0x1f); //FU header, OR 0x80 make start bit enable

        frame_buf  += 1;
        frame_size -= 1;

        while (frame_size + 2 > MAX_RTP_PAYLOAD_SIZE)
        {
            RtpPacket rtp_pkt;

            //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
            rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
            //rtp_pkt.type        = frame.type;
            rtp_pkt.timestamp   = frame.timestamp;
            rtp_pkt.timeNow     = frame.timeNow;
            rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
            rtp_pkt.last        = 0;

            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU_A[0];
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU_A[1];
            memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+2, frame_buf, MAX_RTP_PAYLOAD_SIZE-2);

            /*if (send_frame_callback_)
            {
                if (!send_frame_callback_(channel_id, rtp_pkt))
                    return false;
            }*/
            if (m_pThis && m_pCallMember)
            {
                (m_pThis->*m_pCallMember)(channel_id, rtp_pkt);
            }

            rtp_pkt.data.reset();

            frame_buf  += (MAX_RTP_PAYLOAD_SIZE - 2);
            frame_size -= (MAX_RTP_PAYLOAD_SIZE - 2);

            FU_A[1] &= ~0x80; //FU header (no Start bit)
        }

        //final packet
        {
            RtpPacket rtp_pkt;

            //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
            rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
            //rtp_pkt.type        = frame.type;
            rtp_pkt.timestamp   = frame.timestamp;
            rtp_pkt.timeNow     = frame.timeNow;
            rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2 + frame_size;
            rtp_pkt.last        = 1;

            FU_A[1] |= 0x40; //set the E bit in the FU header
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU_A[0];
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU_A[1];
            memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+2, frame_buf, frame_size);

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
        }
    }

    return true;
}

/*
 https://datatracker.ietf.org/doc/html/rfc3984
 Timestamp: 32 bits
 The RTP timestamp is set to the sampling timestamp of the content.
 A 90 kHz clock rate MUST be used.
 */
void H264Source::GetTimestamp(struct timeval* tv, uint32_t* ts)
{
    /*struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    return ts;*/

    uint32_t timestamp;
    gettimeofday(tv, NULL);
    //*ts = ((tv->tv_sec*1000)+((tv->tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    gettimeofday(tv, NULL);
    //*ts = ((tv->tv_sec*1000)+((tv->tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    timestamp   = (tv->tv_sec + tv->tv_usec/1000000.0)*90000 + 0.5; //90khz sound
    *ts         = htonl(timestamp);
    //uint32_t timestampIncrement = (90000*tv->tv_sec);
    //timestampIncrement += (uint32_t)(90000*(tv->tv_usec/1000000.0) + 0.5);

    /*//auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    chrono::steady_clock::time_point time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    return (uint32_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90 );*/
}
 
