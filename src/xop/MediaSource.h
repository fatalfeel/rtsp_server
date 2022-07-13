// PHZ
// 2018-6-8

#ifndef XOP_MEDIA_SOURCE_H
#define XOP_MEDIA_SOURCE_H

#include "media.h"
#include "rtp.h"
#include "net/Socket.h"
#include <string>
#include <memory>
#include <cstdint>
#include <functional>
#include <map>

namespace xop
{

class MediaSession;

class MediaSource
{
public:
	//using SendFrameCallback = std::function<bool (MediaChannelId channel_id, RtpPacket pkt)>;
	typedef void (MediaSession::*CallMember)(MediaChannelId channel_id, RtpPacket pkt); //ParamInfo is func's paramater

	MediaSource()
	{
	    m_pThis         = NULL;
	    m_pCallMember   = NULL;
	}
	virtual ~MediaSource()
	{}

	virtual MediaType GetMediaType() const
	{ return media_type_; }

	virtual std::string GetMediaDescription(uint16_t port=0) = 0;
	virtual std::string GetAttribute()  = 0;
	virtual bool HandleFrame(MediaChannelId channelId, AVFrame frame) = 0;

	/*virtual void SetSendFrameCallback(SendFrameCallback callback)
	{
	    send_frame_callback_ = callback;
	}*/

	virtual uint32_t GetPayloadType() const
	{ return payload_; }

	virtual uint32_t GetClockRate() const
	{ return clock_rate_; }

	virtual void SetCallMember(MediaSession* inst, CallMember member)
    {
	    m_pThis         = inst;
	    m_pCallMember   = member;
    }

protected:
	MediaType       media_type_ = NONE;
	uint32_t        payload_    = 0;
	uint32_t        clock_rate_ = 0;
	//SendFrameCallback   send_frame_callback_;

	MediaSession*   m_pThis;
	CallMember      m_pCallMember;
};

}

#endif
