#ifndef SEGMENT_H
#define SEGMENT_H

#define HEADER_SIZE 12 //bytes
#define MAX_PAYLOAD_SIZE 1 //bytes

//UDP header
typedef struct _Header{
    unsigned short srcPort;
    unsigned short desPort;
    unsigned short length;
    unsigned short checkSum;
    unsigned int seqNumber;
}Header;

typedef struct _Segment{
    Header header;
    void *payload;
}Segment;

#endif