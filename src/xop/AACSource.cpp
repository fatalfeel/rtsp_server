// PHZ
// 2018-5-16

#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif
#include "AACSource.h"
#include <stdlib.h>
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__)
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

AACSource::AACSource(uint32_t samplerate, uint32_t channels, bool has_adts)
: samplerate_(samplerate)
, channels_(channels)
, has_adts_(has_adts)
{
	payload_    = 97;
	media_type_ = AAC;
	clock_rate_ = samplerate;
}

AACSource* AACSource::CreateNew(uint32_t samplerate, uint32_t channels, bool has_adts)
{
    return new AACSource(samplerate, channels, has_adts);
}

AACSource::~AACSource()
{
}

string AACSource::GetMediaDescription(uint16_t port)
{
	char buf[100] = { 0 };
	sprintf(buf, "m=audio %hu RTP/AVP 97", port); // \r\nb=AS:64
	return string(buf);
}

static uint32_t AACSampleRate[16] =
{
	96000, 88200, 64000, 48000,
	44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000,
	7350, 0, 0, 0 /*reserved */
};

string AACSource::GetAttribute()  // RFC 3640
{
	char buf[500] = { 0 };
	sprintf(buf, "a=rtpmap:97 MPEG4-GENERIC/%u/%u\r\n", samplerate_, channels_);

	uint8_t index = 0;
	for (index = 0; index < 16; index++) {
		if (AACSampleRate[index] == samplerate_) {
			break;
		}        
	}

	if (index == 16) {
		return ""; // error
	}
     
	uint8_t profile = 1;
	char config[10] = {0};

	sprintf(config, "%02x%02x", (uint8_t)((profile+1) << 3)|(index >> 1), (uint8_t)((index << 7)|(channels_<< 3)));
	sprintf(buf+strlen(buf),
			"a=fmtp:97 profile-level-id=1;"
			"mode=AAC-hbr;"
			"sizelength=13;indexlength=3;indexdeltalength=3;"
			"config=%04u",
			atoi(config));

	return string(buf);
}

bool AACSource::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t* frame_buf  = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.timestamp == 0) {
        GetTimestamp(&frame.timeNow, &frame.timestamp);
    }

    /*printf("sendto: %02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n\n",
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
                *((unsigned char*)(frame_buf+11)));*/

    /*if (frame.size > (MAX_RTP_PAYLOAD_SIZE-AU_SIZE)) {
		return false;
	}*/

	int adts_size = 0;
	if (has_adts_) {
		adts_size = ADTS_SIZE;
	}

	/*struct AdtsHeader
	{
	    unsigned int syncword;          //12 bit 同步字 '1111 1111 1111'，说明一个ADTS帧的开始
	    unsigned int id;                //1 bit MPEG 标示符， 0 for MPEG-4，1 for MPEG-2
	    unsigned int layer;             //2 bit 总是'00'
	    unsigned int protectionAbsent;  //1 bit 1表示没有crc，0表示有crc
	    unsigned int profile;           //2 bit 表示使用哪个级别的AAC
	    unsigned int samplingFreqIndex; //4 bit 表示使用的采样频率
	    unsigned int privateBit;        //1 bit
	    unsigned int channelCfg;        //3 bit 表示声道数
	    unsigned int originalCopy;      //1 bit
	    unsigned int home;              //1 bit
	   //下面的为改变的参数即每一帧都不同
	    unsigned int copyrightIdentificationBit;    //1 bit
	    unsigned int copyrightIdentificationStart;  //1 bit
	    unsigned int aacFrameLength;                //13 bit 一个ADTS帧的长度包括ADTS头和AAC原始流
	    unsigned int adtsBufferFullness;            //11 bit 0x7FF 说明是码率可变的码流
	    unsigned int numberOfRawDataBlockInFrame;   //2 bit 有number_of_raw_data_blocks_in_frame + 1个AAC原始帧
	};*/

	frame_buf  +=  adts_size;
	frame_size -=  adts_size;

	char AU[AU_SIZE] = { 0 };
	AU[0] = 0x00;
	AU[1] = 0x10;
	AU[2] = (frame_size & 0x1fe0) >> 5;
	AU[3] = (frame_size & 0x1f) << 3;

	/*RtpPacket rtp_pkt;
	//rtp_pkt.type        = frame.type;
	rtp_pkt.timestamp   = frame.timestamp;
	rtp_pkt.timeNow     = frame.timeNow;
	rtp_pkt.size        = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + AU_SIZE;
	rtp_pkt.last        = 1;

	rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = AU[0];
	rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = AU[1];
	rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = AU[2];
	rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 3] = AU[3];

	memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+AU_SIZE, frame_buf, frame_size);

	if (send_frame_callback_) {
		send_frame_callback_(channel_id, rtp_pkt);
	}*/

	while (frame_size + AU_SIZE > MAX_RTP_PAYLOAD_SIZE)
	{
	    RtpPacket rtp_pkt;

	    //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
	    rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
        //rtp_pkt.type        = frame.type;
        rtp_pkt.timestamp   = frame.timestamp;
        rtp_pkt.timeNow     = frame.timeNow;
        rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE; //3 FU in
        rtp_pkt.last        = 0;

        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = AU[0];
        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = AU[1];
        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = AU[2];
        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 3] = AU[3];

        memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+AU_SIZE, frame_buf, MAX_RTP_PAYLOAD_SIZE-AU_SIZE);

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

        frame_buf  += (MAX_RTP_PAYLOAD_SIZE - AU_SIZE);
        frame_size -= (MAX_RTP_PAYLOAD_SIZE - AU_SIZE);
    }

    //final packet
    {
        RtpPacket rtp_pkt;

        //rtp_pkt.data        = new uint8_t[RTP_PACKET_SIZE];
        rtp_pkt.data        = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);
        //rtp_pkt.type        = frame.type;
        rtp_pkt.timestamp   = frame.timestamp;
        rtp_pkt.timeNow     = frame.timeNow;
        rtp_pkt.size        = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + AU_SIZE + frame_size;
        rtp_pkt.last        = 1;

        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = AU[0];
        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = AU[1];
        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2] = AU[2];
        rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 3] = AU[3];

        memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE+AU_SIZE, frame_buf, frame_size);

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

	return true;
}

/*
 https://datatracker.ietf.org/doc/html/rfc3984
 Timestamp: 32 bits
 The RTP timestamp is set to the sampling timestamp of the content.
 A 90 kHz clock rate MUST be used.
 */
void AACSource::GetTimestamp(struct timeval* tv, uint32_t* ts)
{
    //struct timeval tv = {0};
    //gettimeofday(&tv, NULL);
    //uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    //return ts;

    uint32_t timestamp;
    gettimeofday(tv, NULL);
    //*ts = ((tv->tv_sec*1000)+((tv->tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    timestamp   = (tv->tv_sec + tv->tv_usec/1000000.0)*44100 + 0.5; //44khz sound
    *ts         = htonl(timestamp);

    //uint32_t timestampIncrement = (44100*tv->tv_sec);
    //timestampIncrement += (uint32_t)(44100*(tv->tv_usec/1000000.0) + 0.5);

    /*//auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    chrono::steady_clock::time_point time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
	return (uint32_t)((time_point.time_since_epoch().count()+500) / 1000 * sampleRate / 1000);*/
}
