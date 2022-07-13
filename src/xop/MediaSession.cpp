// PHZ
// 2018-9-30
#include <cstring>
#include <ctime>
#include <map>
#include "MediaSession.h"
#include "RtpConnection.h"
#include "net/Logger.h"
#include "net/SocketUtil.h"

using namespace xop;
using namespace std;

std::atomic_uint MediaSession::last_session_id_(1);

MediaSession::MediaSession(std::string url_suffxx)
	: suffix_(url_suffxx)
	, media_sources_(MAX_MEDIA_CHANNEL)
	, buffer_(MAX_MEDIA_CHANNEL)
{
	has_new_client_ = false;
	session_id_ = ++last_session_id_;

	for(int n=0; n<MAX_MEDIA_CHANNEL; n++) {
		multicast_port_[n] = 0;
	}
}

MediaSession* MediaSession::CreateNew(std::string url_suffix)
{
	return new MediaSession(std::move(url_suffix));
}

MediaSession::~MediaSession()
{
	if (multicast_ip_ != "") {
		MulticastAddr::instance().Release(multicast_ip_);
	}
}

void MediaSession::AddNotifyConnectedCallback(const NotifyConnectedCallback& callback)
{
	notify_connected_callbacks_.push_back(callback);
}

void MediaSession::AddNotifyDisconnectedCallback(const NotifyDisconnectedCallback& callback)
{
	notify_disconnected_callbacks_.push_back(callback);
}

void MediaSession::FrameCallBack(MediaChannelId channel_id, RtpPacket pkt)
{
    std::lock_guard<std::mutex> lock(map_mutex_);

    std::vector<std::shared_ptr<RtpConnection>>                 clients;
    std::map<SOCKET, std::weak_ptr<RtpConnection>>::iterator    mit;

    //for (mit = clients_.begin(); mit != clients_.end();)
    for (mit = clients_.begin(); mit != clients_.end(); mit++)
    {
        //lock() is weak_ptr transfer to shared_ptr
        std::shared_ptr<RtpConnection> conn = mit->second.lock();

        //lock(map_mutex_) has avoid conn to be nullptr
        /*if (conn == nullptr)
        {
            clients_.erase(mit++);
        }
        else
        {
            //event_loop(new xop::EventLoop()) -> EventLoop::Loop() ->
            //EpollTaskScheduler(n) -> ::TaskScheduler(int id)
            //task_schedulers_.push_back(task_scheduler_ptr) already made id >= 0
            int id = conn->GetId();
            if (id >= 0)
                clients.push_back(conn);
            mit++;
        }*/

        if ( conn )
            clients.push_back(conn);
    }

    for(auto& c_iter : clients)
    {
        int ret = 0;
        int id = c_iter->GetId();
        if (id >= 0)
        {
            ret = c_iter->SendRtpPacket(channel_id, pkt);
            if (is_multicast_ && ret == 0)
                break;
        }
    }

    //delete pkt.data;
    //return true;
}

bool MediaSession::AddSource(MediaChannelId channel_id, MediaSource* source)
{
    source->SetCallMember(this, &MediaSession::FrameCallBack);

    /*source->SetSendFrameCallback([&,this](MediaChannelId channel_id, RtpPacket pkt)
	{
        std::lock_guard<std::mutex> lock(map_mutex_);

        std::map<int, RtpPacket>                            packets;
		std::forward_list<std::shared_ptr<RtpConnection>>   clients;
		{
			//std::lock_guard<std::mutex> lock(map_mutex_);
			for (auto iter = clients_.begin(); iter != clients_.end();)
			{
				auto conn = iter->second.lock();
				if (conn == nullptr)
				{
					clients_.erase(iter++);
				}
				else
				{
					int id = conn->GetId();
					if (id >= 0)
					{
						if (packets.find(id) == packets.end())
						{
							RtpPacket tmp_pkt;

							tmp_pkt.data        = pkt.data;
							//memcpy(tmp_pkt.data.get(), pkt.data.get(), pkt.size);
							tmp_pkt.size        = pkt.size;
							tmp_pkt.last        = pkt.last;
							tmp_pkt.timestamp   = pkt.timestamp;
							//tmp_pkt.type        = pkt.type;
							packets.emplace(id, tmp_pkt);
						}
						clients.emplace_front(conn);
					}
					iter++;
				}
			}
		}
        
		for(auto& c_iter : clients)
		{
			int ret = 0;
			int id = c_iter->GetId();
			if (id >= 0)
			{
			    std::map<int, RtpPacket>::iterator p_iter = packets.find(id);
				if (p_iter != packets.end())
				{
					ret = c_iter->SendRtpPacket(channel_id, p_iter->second);
					if (is_multicast_ && ret == 0)
					{
					    break;
					}				
				}
			}					
		}

		delete pkt.data;

		return true;
	});*/

	media_sources_[channel_id].reset(source);

	return true;
}

bool MediaSession::RemoveSource(MediaChannelId channel_id)
{
	media_sources_[channel_id] = nullptr;
	return true;
}

bool MediaSession::StartMulticast()
{  
	if (is_multicast_) {
		return true;
	}

	multicast_ip_ = MulticastAddr::instance().GetAddr();
	if (multicast_ip_ == "") {
		return false;
	}

	std::random_device rd;
	multicast_port_[channel_0] = htons(rd() & 0xfffe);
	multicast_port_[channel_1] = htons(rd() & 0xfffe);

	is_multicast_ = true;
	return true;
}

std::string MediaSession::GetSdpMessage(std::string ip, std::string session_name)
{
	if (sdp_ != "") {
		return sdp_;
	}
    
	if (media_sources_.empty()) {
		return "";
	}

	char buf[2048] = {0};

	snprintf(buf, sizeof(buf),
			"v=0\r\n"
			"o=- 9%ld 1 IN IP4 %s\r\n"
			"t=0 0\r\n"
			"a=control:*\r\n" ,
			(long)std::time(NULL), ip.c_str()); 

	if(session_name != "") {
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
				"s=%s\r\n",
				session_name.c_str());
	}
    
	if(is_multicast_) {
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
				"a=type:broadcast\r\n"
				"a=rtcp-unicast: reflection\r\n");
	}
		
	for (uint32_t chn=0; chn<media_sources_.size(); chn++) {
		if(media_sources_[chn]) {	
			if(is_multicast_) {
				snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
						"%s\r\n",
						media_sources_[chn]->GetMediaDescription(multicast_port_[chn]).c_str()); 
                     
				snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
						"c=IN IP4 %s/255\r\n",
						multicast_ip_.c_str()); 
			}
			else {
				snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
						"%s\r\n",
						media_sources_[chn]->GetMediaDescription(0).c_str());
			}
            
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
					"%s\r\n",
					media_sources_[chn]->GetAttribute().c_str());
                     
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),											
					"a=control:track%d\r\n", chn);	
		}
	}

	sdp_ = buf;
	return sdp_;
}

MediaSource* MediaSession::GetMediaSource(MediaChannelId channel_id)
{
	if (media_sources_[channel_id]) {
		return media_sources_[channel_id].get();
	}

	return nullptr;
}

bool MediaSession::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
	std::lock_guard<std::mutex> lock(mutex_);

	if(media_sources_[channel_id]) {
		media_sources_[channel_id]->HandleFrame(channel_id, frame);
	}
	else {
		return false;
	}

	return true;
}

bool MediaSession::AddClient(SOCKET rtspfd, std::shared_ptr<RtpConnection> rtp_conn)
{
	std::lock_guard<std::mutex> lock(map_mutex_);

	auto iter = clients_.find (rtspfd);
	if(iter == clients_.end()) {
		std::weak_ptr<RtpConnection> rtp_conn_weak_ptr = rtp_conn;
		clients_.emplace(rtspfd, rtp_conn_weak_ptr);
		for (auto& callback : notify_connected_callbacks_) {
			callback(session_id_, rtp_conn->GetIp(), rtp_conn->GetPort());
		}			
        
		has_new_client_ = true;
		return true;
	}
            
	return false;
}

void MediaSession::RemoveClient(SOCKET rtspfd)
{  
	std::lock_guard<std::mutex> lock(map_mutex_);

	auto iter = clients_.find(rtspfd);
	if (iter != clients_.end()) {
		auto conn = iter->second.lock();
		if (conn) {
			for (auto& callback : notify_disconnected_callbacks_) {
				callback(session_id_, conn->GetIp(), conn->GetPort());
			}				
		}
		clients_.erase(iter);
	}
}

