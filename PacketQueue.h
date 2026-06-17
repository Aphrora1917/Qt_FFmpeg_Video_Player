#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include "syncqueue.h"

extern "C"
{
#include "libavcodec/packet.h"
}

typedef struct PacketNode {
    AVPacket *pkt;
    int serial;
} MyPacketNode;

class PacketQueue : public SyncQueue<PacketNode>
{
public:
    PacketQueue() : SyncQueue<PacketNode>(1024) {};

    int nb_packets = 0;
    int size = 1024;
    int64_t duration = 0;
    int abort_request = 0;
    int serial = 0;
};

#endif // PACKETQUEUE_H
