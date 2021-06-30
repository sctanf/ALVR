#ifndef ALVRCLIENT_FEC_H
#define ALVRCLIENT_FEC_H

#include <list>
#include <vector>
#include "packet_types.h"
#include "reedsolomon/rs.h"

class FECQueue {
public:
    FECQueue();
    ~FECQueue();

    void addVideoPacket(const VideoFrame *packet, int packetSize, bool &fecFailure);
    bool reconstruct();
    const std::byte *getFrameBuffer();
    int getFrameByteSize();
private:

    VideoFrame m_currentFrame;
    size_t m_packets;
    uint32_t m_receivedPackets;
    std::vector<unsigned char> m_marks;
    std::vector<std::byte> m_frameBuffer;
    bool m_recovered;
};

#endif //ALVRCLIENT_FEC_H
