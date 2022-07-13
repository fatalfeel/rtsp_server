#ifndef XOP_RTSP_SENDREPORT_H
#define XOP_RTSP_SENDREPORT_H

namespace xop
{

enum
{
    // SDES tags:
    RTCP_SDES_END   = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME  = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC   = 5,
    RTCP_SDES_TOOL  = 6,
    RTCP_SDES_NOTE  = 7,
    RTCP_SDES_PRIV  = 8,
    // RTCP packet types:
    RTCP_PT_SR      = 200,
    RTCP_PT_RR      = 201,
    RTCP_PT_SDES    = 202,
    RTCP_PT_BYE     = 203,
    RTCP_PT_APP     = 204,
    RTCP_PT_RTPFB   = 205, // Generic RTP Feedback [RFC4585]
    RTCP_PT_PSFB    = 206, // Payload-specific [RFC4585]
    RTCP_PT_XR      = 207, // extended report [RFC3611]
    RTCP_PT_AVB     = 208, // AVB RTCP packet ["Standard for Layer 3 Transport Protocol for Time Sensitive Applications in Local Area Networks." Work in progress.]
    RTCP_PT_RSI     = 209, // Receiver Summary Information [RFC5760]
    RTCP_PT_TOKEN   = 210, // Port Mapping [RFC6284]
    RTCP_PT_IDMS    = 211  // IDMS Settings [RFC7272]
};

class SDESItem
{
public:
  SDESItem();
  ~SDESItem();
  void Init(unsigned char tag, char* name);

  unsigned char*    data();
  unsigned int      totalSize();

private:
  unsigned char fData[2 + 0xFF]; // first 2 bytes are tag and length
};

class SenderReport
{
public:
    SenderReport();
    ~SenderReport();
    void addSR(uint32_t ssrc, struct timeval timeNow, uint32_t rtpTimestamp);
    void addSDES(uint32_t ssrc);
    void enqueueCommonReportPrefix(uint8_t  packetType, uint32_t ssrc, uint32_t numExtraWords);
    void enqueuedWord(uint32_t dWord);
    void enqueue(uint8_t* from, uint32_t numBytes);

    uint8_t     m_fOutBuf[64];
    uint32_t    m_fCurOffset;
    SDESItem    m_fCNAME;
};

}

#endif
