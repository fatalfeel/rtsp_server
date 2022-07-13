// PHZ
// 2018-9-30
// RRTP packet header payload type
// https://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-2

#include "RtpConnection.h"
#include "RtspConnection.h"
#include "net/SocketUtil.h"
#include "SenderReport.h"

using namespace std;
using namespace xop;

RtpConnection::RtpConnection(std::weak_ptr<TcpConnection> wtcp_connection)
: wtcp_connection_(wtcp_connection)
{
	std::random_device rd;

	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		rtpfd_[chn] = 0;
		rtcpfd_[chn] = 0;
		memset(&media_channel_info_[chn], 0, sizeof(media_channel_info_[chn]));
		media_channel_info_[chn].rtp_header.version = RTP_VERSION;
		media_channel_info_[chn].packet_seq         = rd()&0xffff;
		media_channel_info_[chn].rtp_header.seq     = 0; //htons(1);
		media_channel_info_[chn].rtp_header.ts      = htonl(rd());
		media_channel_info_[chn].rtp_header.ssrc    = htonl(rd());
	}

	//lock() is weak_ptr transfer to shared_ptr
	std::shared_ptr<TcpConnection> conn = wtcp_connection_.lock();
	rtsp_ip_ = conn->GetIp();
	rtsp_port_ = conn->GetPort();
}

RtpConnection::~RtpConnection()
{
	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		if(rtpfd_[chn] > 0) {
			SocketUtil::Close(rtpfd_[chn]);
		}

		if(rtcpfd_[chn] > 0) {
			SocketUtil::Close(rtcpfd_[chn]);
		}
	}
}

int RtpConnection::GetId() const
{
    int id = -1;

    //lock() is weak_ptr transfer to shared_ptr
    std::shared_ptr<TcpConnection> conn = wtcp_connection_.lock();
	if (!conn) {
		return id;
	}

	//even id is correct but wrong using
	//RtspConnection* rtspConn = (RtspConnection*)conn.get();
	//return rtspConn->GetId();

	TaskScheduler* tasksch = conn->GetTaskScheduler(); //same as rtspConn->GetTaskScheduler()
	if( tasksch )
	    id = tasksch->GetId();

	return id;
}

bool RtpConnection::SetupRtpOverTcp(MediaChannelId channel_id, uint16_t rtp_channel, uint16_t rtcp_channel)
{
    //lock() is weak_ptr transfer to shared_ptr
    std::shared_ptr<TcpConnection> conn = wtcp_connection_.lock();
	if (!conn) {
		return false;
	}

	media_channel_info_[channel_id].rtp_channel = rtp_channel;
	media_channel_info_[channel_id].rtcp_channel = rtcp_channel;
	rtpfd_[channel_id] = conn->GetSocket();
	rtcpfd_[channel_id] = conn->GetSocket();
	media_channel_info_[channel_id].is_setup = true;
	transport_mode_ = RTP_OVER_TCP;

	return true;
}

bool RtpConnection::SetupRtpOverUdp(MediaChannelId channel_id, uint16_t rtp_port, uint16_t rtcp_port)
{
    //lock() is weak_ptr transfer to shared_ptr
    std::shared_ptr<TcpConnection> conn = wtcp_connection_.lock();
	if (!conn) {
		return false;
	}

	if(SocketUtil::GetPeerAddr(conn->GetSocket(), &peer_addr_) < 0) {
		return false;
	}

	media_channel_info_[channel_id].rtp_port = rtp_port;
	media_channel_info_[channel_id].rtcp_port = rtcp_port;

	std::random_device rd;
	for (int n = 0; n <= 10; n++) {
		if (n == 10) {
			return false;
		}
        
		local_rtp_port_[channel_id] = rd() & 0xfffe;
		local_rtcp_port_[channel_id] =local_rtp_port_[channel_id] + 1;

		rtpfd_[channel_id] = ::socket(AF_INET, SOCK_DGRAM, 0);
		if(!SocketUtil::Bind(rtpfd_[channel_id], "0.0.0.0",  local_rtp_port_[channel_id])) {
			SocketUtil::Close(rtpfd_[channel_id]);
			continue;
		}

		rtcpfd_[channel_id] = ::socket(AF_INET, SOCK_DGRAM, 0);
		if(!SocketUtil::Bind(rtcpfd_[channel_id], "0.0.0.0", local_rtcp_port_[channel_id])) {
			SocketUtil::Close(rtpfd_[channel_id]);
			SocketUtil::Close(rtcpfd_[channel_id]);
			continue;
		}

		break;
	}

	SocketUtil::SetSendBufSize(rtpfd_[channel_id], 50*1024);

	peer_rtp_addr_[channel_id].sin_family = AF_INET;
	peer_rtp_addr_[channel_id].sin_addr.s_addr = peer_addr_.sin_addr.s_addr;
	peer_rtp_addr_[channel_id].sin_port = htons(media_channel_info_[channel_id].rtp_port);

	peer_rtcp_sddr_[channel_id].sin_family = AF_INET;
	peer_rtcp_sddr_[channel_id].sin_addr.s_addr = peer_addr_.sin_addr.s_addr;
	peer_rtcp_sddr_[channel_id].sin_port = htons(media_channel_info_[channel_id].rtcp_port);

	media_channel_info_[channel_id].is_setup = true;
	transport_mode_ = RTP_OVER_UDP;

	return true;
}

bool RtpConnection::SetupRtpOverMulticast(MediaChannelId channel_id, std::string ip, uint16_t port)
{
    std::random_device rd;
    for (int n = 0; n <= 10; n++) {
		if (n == 10) {
			return false;
		}
       
		local_rtp_port_[channel_id] = rd() & 0xfffe;
		rtpfd_[channel_id] = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (!SocketUtil::Bind(rtpfd_[channel_id], "0.0.0.0", local_rtp_port_[channel_id])) {
			SocketUtil::Close(rtpfd_[channel_id]);
			continue;
		}

		break;
    }

	media_channel_info_[channel_id].rtp_port = port;

	peer_rtp_addr_[channel_id].sin_family = AF_INET;
	peer_rtp_addr_[channel_id].sin_addr.s_addr = inet_addr(ip.c_str());
	peer_rtp_addr_[channel_id].sin_port = htons(port);

	media_channel_info_[channel_id].is_setup = true;
	transport_mode_ = RTP_OVER_MULTICAST;
	is_multicast_ = true;
	return true;
}

void RtpConnection::Play()
{
	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++)
	{
		if (media_channel_info_[chn].is_setup)
		{
			media_channel_info_[chn].is_play        = true;
			media_channel_info_[chn].m_startplay    = true;
		}
	}
}

void RtpConnection::Record()
{
	for (int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		if (media_channel_info_[chn].is_setup) {
			media_channel_info_[chn].is_record = true;
			media_channel_info_[chn].is_play = true;
		}
	}
}

void RtpConnection::Teardown()
{
	if(!is_closed_) {
		is_closed_ = true;
		for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
			media_channel_info_[chn].is_play = false;
			media_channel_info_[chn].is_record = false;
		}
	}
}

string RtpConnection::GetMulticastIp(MediaChannelId channel_id) const
{
	return std::string(inet_ntoa(peer_rtp_addr_[channel_id].sin_addr));
}

string RtpConnection::GetRtpInfo(const std::string& rtsp_url)
{
	char buf[2048] = { 0 };
	snprintf(buf, 1024, "RTP-Info: ");

	int num_channel = 0;

	auto time_point = chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now());
	auto ts = time_point.time_since_epoch().count();
	for (int chn = 0; chn<MAX_MEDIA_CHANNEL; chn++) {
		uint32_t rtpTime = (uint32_t)(ts*media_channel_info_[chn].clock_rate / 1000);
		if (media_channel_info_[chn].is_setup) {
			if (num_channel != 0) {
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",");
			}			

			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
					"url=%s/track%d;seq=0;rtptime=%u",
					rtsp_url.c_str(), chn, rtpTime);
			num_channel++;
		}
	}

	return std::string(buf);
}

/*void RtpConnection::SetFrameType(uint8_t frame_type)
{
	frame_type_ = frame_type;
	if(!has_key_frame_ && (frame_type == 0 || frame_type == VIDEO_FRAME_I)) {
		has_key_frame_ = true;
	}
}*/

void RtpConnection::SetRtpHeader(MediaChannelId channel_id, RtpPacket pkt)
{
    //if((media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record) && has_key_frame_) {
    if(media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record) {
        media_channel_info_[channel_id].rtp_header.marker   = pkt.last;
        //media_channel_info_[channel_id].rtp_header.ts       = htonl(pkt.timestamp); //already done in GetTimestamp
        media_channel_info_[channel_id].rtp_header.ts       = pkt.timestamp;
		media_channel_info_[channel_id].rtp_header.seq      = htons(media_channel_info_[channel_id].packet_seq++);
		memcpy(pkt.data.get()+RTP_TCP_HEAD_SIZE, &media_channel_info_[channel_id].rtp_header, RTP_HEADER_SIZE);
	}
}

int RtpConnection::SendRtpPacket(MediaChannelId channel_id, RtpPacket pkt)
{    
	int ret = -1;

    if (is_closed_) {
		return ret;
	}
   
    //lock() is weak_ptr transfer to shared_ptr
    std::shared_ptr<TcpConnection> conn = wtcp_connection_.lock();
	if (!conn) {
		return ret;
	}

	//this->SetFrameType(pkt.type);
	//this->SetRtpHeader(channel_id, pkt);

    //sender report
    if( media_channel_info_[channel_id].m_startplay )
    {
        media_channel_info_[channel_id].m_startplay = false;

        //printf("SR packet: SendRtpPacket\n"); //debug

        SenderReport* syncntp = new SenderReport();

        if(transport_mode_ == RTP_OVER_TCP)
        {
            syncntp->addSR(media_channel_info_[channel_id].rtp_header.ssrc, pkt.timeNow, pkt.timestamp);
            syncntp->addSDES(media_channel_info_[channel_id].rtp_header.ssrc);

            RtpPacket temp;
            temp.data = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);

            memcpy(temp.data.get(), syncntp->m_fOutBuf, syncntp->m_fCurOffset);
            temp.size   = syncntp->m_fCurOffset;
            SendRtpOverTcp(channel_id, temp);

            temp.data.reset();
        }
        else
        {
            syncntp->addSR(media_channel_info_[channel_id].rtp_header.ssrc, pkt.timeNow, pkt.timestamp);
            syncntp->addSDES(media_channel_info_[channel_id].rtp_header.ssrc);

            RtpPacket temp;
            temp.data = std::shared_ptr<uint8_t>(new uint8_t[RTP_PACKET_SIZE]);

            memcpy(temp.data.get(), syncntp->m_fOutBuf, syncntp->m_fCurOffset);
            temp.size   = syncntp->m_fCurOffset;
            SendRtpOverUdp(channel_id, temp);

            temp.data.reset();
        }

        delete syncntp;
    }

    //if((media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record) && has_key_frame_ )
    if(media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record)
    {
        /*bool            bres;
        RtspConnection* rtsp_conn;

        rtsp_conn   = (RtspConnection *)conn.get();
        bres        = rtsp_conn->task_scheduler_->AddTriggerEvent([this, channel_id, pkt]
                                                                   {
                                                                       this->SetRtpHeader(channel_id, pkt);

                                                                       if(transport_mode_ == RTP_OVER_TCP)
                                                                           SendRtpOverTcp(channel_id, pkt);
                                                                       else
                                                                           SendRtpOverUdp(channel_id, pkt);
                                                                   });*/
        this->SetRtpHeader(channel_id, pkt);

        if(transport_mode_ == RTP_OVER_TCP)
           SendRtpOverTcp(channel_id, pkt);
        else
           SendRtpOverUdp(channel_id, pkt);

        ret = 0;
    }

	/*RtspConnection* rtsp_conn = (RtspConnection *)conn.get();
	bool ret = rtsp_conn->task_scheduler_->AddTriggerEvent([this, channel_id, pkt]
    {
		//this->SetFrameType(pkt.type);
		this->SetRtpHeader(channel_id, pkt);

		if((media_channel_info_[channel_id].is_play || media_channel_info_[channel_id].is_record) && has_key_frame_ )
		{
			if(transport_mode_ == RTP_OVER_TCP)
			{
			    SendRtpOverTcp(channel_id, pkt);
			}
			else
			{
			    SendRtpOverUdp(channel_id, pkt);
			}
		}
	});*/

	return ret;
}

int RtpConnection::SendRtpOverTcp(MediaChannelId channel_id, RtpPacket pkt)
{
    //lock() is weak_ptr transfer to shared_ptr
    std::shared_ptr<TcpConnection> conn = wtcp_connection_.lock();
	if (!conn) {
		return -1;
	}

	uint8_t* rtpPktPtr = pkt.data.get();
	rtpPktPtr[0] = '$';
	rtpPktPtr[1] = (char)media_channel_info_[channel_id].rtp_channel;
	rtpPktPtr[2] = (char)(((pkt.size-RTP_TCP_HEAD_SIZE)&0xFF00)>>8);
	rtpPktPtr[3] = (char)((pkt.size -RTP_TCP_HEAD_SIZE)&0xFF);

	conn->Send((char*)rtpPktPtr, pkt.size);

	return pkt.size;
}

int RtpConnection::SendRtpOverUdp(MediaChannelId channel_id, RtpPacket pkt)
{
    //uint8_t* buffer = pkt.data.get();
    //debug
    //printf("seq: %04x\n",   ntohs(*((unsigned short*)(buff+2))));
    //printf("tim: %04d\n\n", ntohl(*((unsigned int*)(buff+4))));
    //sender report
    /*if( *((unsigned char*)(buffer+12)) != 0x00
        &&
        *((unsigned char*)(buffer+12)) != 0x02
        &&
        *((unsigned char*)(buffer+12)) != 0x10
        &&
        *((unsigned char*)(buffer+12)) != 0x40
        &&
        *((unsigned char*)(buffer+12)) != 0x42
        &&
        *((unsigned char*)(buffer+12)) != 0x42
        &&
        *((unsigned char*)(buffer+12)) != 0x44
        &&
        *((unsigned char*)(buffer+12)) != 0x62 )
        printf("SR packet: writeSocket\n");*/

    /*printf("sendto: %02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n\n",
            *((unsigned char*)(buffer+12)),
            *((unsigned char*)(buffer+13)),
            *((unsigned char*)(buffer+14)),
            *((unsigned char*)(buffer+15)),
            *((unsigned char*)(buffer+16)),
            *((unsigned char*)(buffer+17)),
            *((unsigned char*)(buffer+18)),
            *((unsigned char*)(buffer+19)),
            *((unsigned char*)(buffer+20)),
            *((unsigned char*)(buffer+21)),
            *((unsigned char*)(buffer+22)),
            *((unsigned char*)(buffer+23)));*/

    int ret = sendto(rtpfd_[channel_id],
	                (const char*)pkt.data.get()+RTP_TCP_HEAD_SIZE,
	                pkt.size-RTP_TCP_HEAD_SIZE,
	                0,
					(struct sockaddr *)&(peer_rtp_addr_[channel_id]),
					sizeof(struct sockaddr_in));

    if(ret < 0) {
		Teardown();
		return -1;
	}

	return ret;
}
