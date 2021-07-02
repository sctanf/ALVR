#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <inttypes.h>
#include "fec.h"
#include "packet_types.h"
#include "utils.h"

FECQueue::FECQueue() {
    m_currentFrame.videoFrameIndex = UINT64_MAX;
    m_recovered = true;
}

FECQueue::~FECQueue() {
}

// Add packet to queue. packet must point to buffer whose size=ALVR_MAX_PACKET_SIZE.
void FECQueue::addVideoPacket(const VideoFrame *packet, int packetSize, bool &fecFailure) {
    if (m_recovered && m_currentFrame.videoFrameIndex == packet->videoFrameIndex) {
        return;
    }
    if (m_currentFrame.videoFrameIndex != packet->videoFrameIndex) {
        // New frame
        m_currentFrame = *packet;
        m_recovered = false;

        m_packets = m_currentFrame.frameByteSize / ALVR_MAX_VIDEO_BUFFER_SIZE + 1;
        m_receivedPackets = 0;

        m_marks.resize(m_packets);
        memset(&m_marks[0], 1, m_packets);

        if (m_frameBuffer.size() < m_currentFrame.frameByteSize) {
            // Only expand buffer for performance reason.
            m_frameBuffer.resize(m_currentFrame.frameByteSize);
        }
        memset(&m_frameBuffer[0], 0, m_currentFrame.frameByteSize);
    }
    if (m_marks[packet->fecIndex] == 0) {
        // Duplicate packet.
        LOGI("Packet duplication. packetCounter=%d fecIndex=%d", packet->packetCounter,
             packet->fecIndex);
        return;
    }
    m_marks[packet->fecIndex] = 0;
    m_receivedPackets++;

    std::byte *p = &m_frameBuffer[packet->fecIndex * ALVR_MAX_VIDEO_BUFFER_SIZE];
    char *payload = ((char *) packet) + sizeof(VideoFrame);
    int payloadSize = packetSize - sizeof(VideoFrame);
    memcpy(p, payload, payloadSize);
}

bool FECQueue::reconstruct() {
    if (m_recovered) {
        return false;
    }

    bool ret = true;
    // On server side, we encoded all buffer in one call of reed_solomon_encode.
    // But client side, we should split shards for more resilient recovery.
    if (m_receivedPackets < m_packets) {
        // Not enough parity data
        ret = false;
    }
    if (ret) {
        m_recovered = true;
        FrameLog(m_currentFrame.trackingFrameIndex, "Frame was successfully recovered by FEC.");
    }
    return ret;
}

const std::byte *FECQueue::getFrameBuffer() {
    return &m_frameBuffer[0];
}

int FECQueue::getFrameByteSize() {
    return m_currentFrame.frameByteSize;
}
