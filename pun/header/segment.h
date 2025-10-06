#ifndef SEGMENT_H
#define SEGMENT_H

#define TYPE_REQUEST 1
#define TYPE_RESPONSE 2
#define TYPE_DATA 3
#define TYPE_ACK 4
#define TYPE_NAK 5
#define TYPE_COMPLETE 6

struct MetaData {
    int type;
    char filename[256];
    bool fileExists;
    int fileSize;
    int totalSegments;
    int windowSize;
    int maxPayloadSize;
};

#define HEADER_SIZE 12 //bytes
#define _MAX_PAYLOAD_SIZE 1
#define MAX_PAYLOAD_SIZE ( _MAX_PAYLOAD_SIZE < sizeof(MetaData) ? sizeof(MetaData) : (_MAX_PAYLOAD_SIZE > 1024 ? 1024 : _MAX_PAYLOAD_SIZE )) //bytes
// limit MAX_PAYLOAD_SIZE to sizeof(MataData) ~ 280 - 1024 bytes

//UDP header
typedef struct _Header{
    unsigned short srcPort;
    unsigned short desPort;
    unsigned short length;
    unsigned short checkSum;
    int seqNumber;
}Header;

typedef struct _Segment{
    Header header;
    void *payload;
}Segment;

#endif