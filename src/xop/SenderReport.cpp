/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2022 Live Networks, Inc.  All rights reserved.
// RTCP
// Implementation

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "SenderReport.h"

namespace xop
{

SDESItem::SDESItem()
{
}

SDESItem::~SDESItem()
{
}

void SDESItem::Init(unsigned char tag, char* name)
{
  unsigned int length = strlen(name);
  if (length > 0xFF)
      length = 0xFF; // maximum data length for a SDES item

  fData[0] = tag;
  fData[1] = (unsigned char)length;

  memcpy(&fData[2], name, length);
}

uint32_t SDESItem::totalSize()
{
  return 2 + (unsigned)fData[1];
}

unsigned char* SDESItem::data()
{
  return fData;
}

/////////////////////////////////////////////////////////
SenderReport::SenderReport()
{
    char cname[256] = "";

    m_fCurOffset = 0;
    memset(m_fOutBuf, 0, sizeof(m_fOutBuf));

    gethostname(cname, sizeof(cname));
    m_fCNAME.Init(RTCP_SDES_CNAME, cname);
}

SenderReport::~SenderReport()
{
}

void SenderReport::addSR(uint32_t ssrc, struct timeval timeNow, uint32_t rtpTimestamp)
{
    //struct timeval timeNow;
    enqueueCommonReportPrefix(RTCP_PT_SR, ssrc, 5);

    // Insert the NTP and RTP timestamps for the 'wallclock time'
    //gettimeofday(&timeNow, NULL);
    enqueuedWord(timeNow.tv_sec + 0x83AA7E80);

    // NTP timestamp most-significant word (1970 epoch -> 1900 epoch)
    double fractionalPart = (timeNow.tv_usec/15625.0)*0x04000000; // 2^32/10^6
    enqueuedWord((uint32_t)(fractionalPart+0.5));

    // NTP timestamp least-significant word
    //uint32_t rtpTimestamp = fSink->convertToRTPTimestamp(timeNow);
    enqueuedWord(rtpTimestamp); // RTP ts

    // Insert the packet and byte counts:
    enqueuedWord(0);
    enqueuedWord(0);
}

void SenderReport::addSDES(uint32_t ssrc)
{
    unsigned int numBytes = 4;

    // counts the SSRC, but not the header; it'll get subtracted out
    numBytes += m_fCNAME.totalSize();   // includes id and length
    numBytes += 1;                      // the special END item

    unsigned num4ByteWords = (numBytes + 3)/4;

    unsigned rtcpHdr = 0x81000000; // version 2, no padding, 1 SSRC chunk
    rtcpHdr |= (RTCP_PT_SDES<<16);
    rtcpHdr |= num4ByteWords;
    enqueuedWord(rtcpHdr);

    enqueuedWord(ssrc);

    // Add the CNAME:
    enqueue(m_fCNAME.data(), m_fCNAME.totalSize());

    // Add the 'END' item (i.e., a zero byte), plus any more needed to pad:
    unsigned int    numPaddingBytesNeeded = 4 - (m_fCurOffset % 4);
    unsigned char   zero = '\0';
    while (numPaddingBytesNeeded-- > 0)
        enqueue(&zero, 1);
}

void SenderReport::enqueueCommonReportPrefix(uint8_t  packetType, uint32_t ssrc, uint32_t numExtraWords)
{
    uint32_t numReportingSources = 0;       //we don't receive anything
    uint32_t rtcpHdr = 0x80000000;          // version 2, no padding
    rtcpHdr |= (numReportingSources<<24);
    rtcpHdr |= (packetType<<16);
    rtcpHdr |= (1 + numExtraWords + 6*numReportingSources);

    //each report block is 6 32-bit words long
    enqueuedWord(rtcpHdr);
    enqueuedWord(ssrc);
}

void SenderReport::enqueuedWord(uint32_t dword)
{
    uint32_t nvalue = htonl(dword);
    enqueue((unsigned char*)&nvalue, sizeof(uint32_t));
}

void SenderReport::enqueue(uint8_t* from, uint32_t numBytes)
{
    memcpy(m_fOutBuf+m_fCurOffset, from, numBytes);
    m_fCurOffset += numBytes;
}

}
