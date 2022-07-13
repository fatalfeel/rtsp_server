// PHZ
// 2018-6-7
// http://ffmpeg.org/doxygen/trunk/rtpenc__hevc_8c_source.html
// https://www.twblogs.net/a/5e554018bd9eee211684990e

#if defined(WIN32) || defined(_WIN32) 
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "H265Source.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__) 
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

H265Source::H265Source(uint32_t framerate)
: framerate_(framerate)
{
	payload_    = 96;
	media_type_ = H265;
	clock_rate_ = 90000;
}

H265Source* H265Source::CreateNew(uint32_t framerate)
{
	return new H265Source(framerate);
}

H265Source::~H265Source()
{
	
}

string H265Source::GetMediaDescription(uint16_t port)
{
	char buf[100] = {0};
	sprintf(buf, "m=video %hu RTP/AVP 96", port);
	return string(buf);
}
	
string H265Source::GetAttribute()
{
	return string("a=rtpmap:96 H265/90000");
}

bool H265Source::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
	uint8_t *frame_buf  = frame.buffer.get();
	uint32_t frame_size = frame.size;

	if (frame.timestamp == 0) {
	    GetTimestamp(&frame.timeNow, &frame.timestamp);
	}

	/*printf("ReadFrame3: %02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n",
            *((unsigned char*)(frame_buf+0)),
            *((unsigned char*)(frame_buf+1)),
            *((unsigned char*)(frame_buf+2)),
            *((unsigned char*)(frame_buf+3)),
            *((unsigned char*)(frame_buf+4)),
            *((unsigned char*)(frame_buf+5)),
            *((unsigned char*)(frame_buf+6)),
            *((unsigned char*)(frame_buf+7)),
            *((unsigned char*)(frame_buf+8)),
            *((unsigned char*)(frame_buf+9)),
            *((unsigned char*)(frame_buf+10)),
            *((unsigned char*)(frame_buf+11)));

	if( *((unsigned char*)(frame_buf+0)) == 0x00
                &&
        *((unsigned char*)(frame_buf+1)) == 0x00
                &&
        *((unsigned char*)(frame_buf+2)) == 0x00
                &&
        *((unsigned char*)(frame_buf+3)) == 0x01
                &&
        *((unsigned char*)(frame_buf+4)) == 0x00
                &&
        *((unsigned char*)(frame_buf+5)) == 0x01
                &&
        *((unsigned char*)(frame_buf+6)) == 0xfe
                &&
        *((unsigned char*)(frame_buf+7)) == 0xe2
                &&
        *((unsigned char*)(frame_buf+8)) == 0x2d
                &&
        *((unsigned char*)(frame_buf+9)) == 0x57
                &&
        *((unsigned char*)(frame_buf+10)) == 0xf7
                &&
        *((unsigned char*)(frame_buf+11)) == 0x08)
        printf("found3\n\n");*/
        
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
	    //fixed for VLC [hevc @ 0x7fc85c04b680] Invalid NAL unit 0, skipping.
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

		//debug
		/*printf("case 1:");
        for(int i=0; i<frame_size; i++)
        {
            printf("%02x ", rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+i]);
        }
        printf("\n");*/
        
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
         7 6 5 4 3 2 1 0
        +-+-+-+-+-+-+-+-+
        |F|    Type     |
        +---------------+
        Type 49 is fragmentation unit
         7 6 5 4 3 2 1 0
        +-+-+-+-+-+-+-+-+
        | LayerId | TID |
        +---------------+
         7 6 5 4 3 2 1 0
        +-+-+-+-+-+-+-+-+
        |S|E|  FuType   |
        +---------------+
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

	    //refer live555 void H264or5Fragmenter::doGetNextFrame() of H264or5VideoRTPSink.cpp
        //char FU[3] = {0};
        //char nalUnitType = (frame_buf[0] & 0x7E) >> 1;
	    uint8_t PL_FU[3]    = {0}; //payload 2 bytes, FU 1 byte
	    uint8_t nalUnitType = (frame_buf[0] & 0x7e) >> 1;
		PL_FU[0] = (frame_buf[0] & 0x81) | (49<<1);
		PL_FU[1] = frame_buf[1];
		PL_FU[2] = 0x80 | nalUnitType;
        
		frame_buf  += 2;
		frame_size -= 2;
        
		while (frame_size + 3 > MAX_RTP_PAYLOAD_SIZE)
		{
			RtpPacket rtp_pkt;

			//rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
			rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
			//rtp_pkt.type        = frame.type;
			rtp_pkt.timestamp   = frame.timestamp;
			rtp_pkt.timeNow     = frame.timeNow;
			rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE; //3 FU in
			rtp_pkt.last        = 0;

			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = PL_FU[0];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = PL_FU[1];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = PL_FU[2];

			memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+3, frame_buf, MAX_RTP_PAYLOAD_SIZE-3);

			//debug
			/*printf("case 2:");
	        for(int i=0; i<MAX_RTP_PAYLOAD_SIZE; i++)
	        {
	            printf("%02x ", rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+i]);
	        }
	        printf("\n");*/
            
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
            
			frame_buf  += (MAX_RTP_PAYLOAD_SIZE - 3);
			frame_size -= (MAX_RTP_PAYLOAD_SIZE - 3);
        
			PL_FU[2] &= ~0x80; //FU header (no Start bit)
		}
        
		//final packet
		{
		    RtpPacket rtp_pkt;

		    //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
		    rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
			//rtp_pkt.type        = frame.type;
			rtp_pkt.timestamp   = frame.timestamp;
			rtp_pkt.timeNow     = frame.timeNow;
			rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 3 + frame_size;
			rtp_pkt.last        = 1;

			PL_FU[2] |= 0x40; //set the E bit in the FU header
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = PL_FU[0];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = PL_FU[1];
			rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = PL_FU[2];

			memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+3, frame_buf, frame_size);

			//debug
			/*printf("case 3:");
            for(int i=0; i<3+frame_size; i++)
            {
                printf("%02x ", rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+i]);
            }
            printf("\n\n");*/
            
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
void H265Source::GetTimestamp(struct timeval* tv, uint32_t* ts)
{
	/*struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
	return ts;*/

    uint32_t timestamp;
    gettimeofday(tv, NULL);
    //*ts = ((tv->tv_sec*1000)+((tv->tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    timestamp   = (tv->tv_sec + tv->tv_usec/1000000.0)*90000 + 0.5; //90khz sound
    *ts         = htonl(timestamp);

	/*
    //auto time_point = chrono::time_point_cast<chrono::milliseconds>(chrono::system_clock::now());
	//auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    chrono::steady_clock::time_point time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
	return (uint32_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90);
	*/
}
