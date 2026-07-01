#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include "syncqueue.h"

extern "C"
{
#include "libavcodec/packet.h"
}

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25

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

    int packet_queue_put(AVPacket *pkt)
    {
        AVPacket *pkt1;
        int ret;

        pkt1 = av_packet_alloc();
        if (!pkt1) {
            av_packet_unref(pkt);
            return -1;
        }
        av_packet_move_ref(pkt1, pkt);

        ret = packet_queue_put_private(pkt1);

        if (ret < 0)
            av_packet_free(&pkt1);

        return ret;
    }

    int packet_queue_put_nullpacket(AVPacket *pkt, int stream_index)
    {
        pkt->stream_index = stream_index;
        return packet_queue_put(pkt);
    }

    void packet_queue_flush()
    {
        PacketNode pkt1;
        while (!isEmpty())
        {
            pkt1 = dequeue();
            av_packet_free(&pkt1.pkt);
        }

        this->nb_packets = 0;
        this->size = 0;
        this->duration = 0;
        this->serial++;
    }

private:
    int packet_queue_put_private(AVPacket *pkt)
    {
        PacketNode pkt1;
        int ret;

        if (this->abort_request)
            return -1;


        pkt1.pkt = pkt;
        pkt1.serial = this->serial;

        enqueue(pkt1);

        this->nb_packets++;
        this->size += pkt1.pkt->size + sizeof(pkt1);
        this->duration += pkt1.pkt->duration;

        return 0;
    }
};

#endif // PACKETQUEUE_H
