// shared_defs.hpp (วางไว้บนสุดของทั้งสองไฟล์หรือแยกไฟล์ก็ได้)
#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

using namespace std;

#define HEADER_SIZE 12            // bytes (2*4 + 4)
#define MAX_PAYLOAD_SIZE 1450     // bytes (ปรับได้; ต้องเท่ากันทั้งสองฝั่ง)

#pragma pack(push, 1)
typedef struct _Header {
    unsigned short srcPort;
    unsigned short desPort;
    unsigned short length;    // header + payload
    unsigned short checkSum;  // (ยังไม่คำนวณ)
    unsigned int   seqNumber;
} Header;
#pragma pack(pop)

typedef struct _Segment {
    Header header;
    void*  payload;  // ชี้ไปยังบัฟเฟอร์ (ระวังอายุของหน่วยความจำ)
} Segment;

// ---------- Utilities (binary safe) ----------
pair<char*, int> readFile(const string& fileName) {
    ifstream file(fileName, ios::binary);
    if (!file.is_open()) return {nullptr, 0};
    file.seekg(0, ios::end);
    streamsize fsize = file.tellg();
    file.seekg(0, ios::beg);
    char* s = (fsize > 0) ? new char[fsize] : nullptr;
    if (fsize > 0) file.read(s, fsize);
    return {s, (int)fsize};
}

bool writeFile(const string& fileName, const char* payload, int fileSize) {
    ofstream file(fileName, ios::binary);
    if (!file.is_open()) return false;
    if (fileSize > 0) file.write(payload, fileSize);
    return true;
}

Segment* createSegment(void* payload, int part, int length) {
    Segment* seg = new Segment;
    seg->header.srcPort = 0;
    seg->header.desPort = 0;
    seg->header.length  = HEADER_SIZE + (unsigned short)length;
    seg->header.seqNumber = (unsigned int)part;
    seg->header.checkSum  = 0;
    seg->payload = payload; // ชี้ไปยังบัฟเฟอร์ (ไม่ copy)
    return seg;
}

vector<Segment*> packetize(void* file, int fileSize, int limitByte) {
    int part = (fileSize + limitByte - 1) / limitByte;
    vector<Segment*> segments(part);
    for (int p = 0; p < part; ++p) {
        int l_idx = p * limitByte;
        void* cur = (void*)((char*)file + l_idx);
        int length = (p == part - 1 && (fileSize % limitByte) ? (fileSize % limitByte) : limitByte);
        segments[p] = createSegment(cur, p, length);
    }
    return segments;
}

// resemble แบบเดิม (พึ่งพา MAX_PAYLOAD_SIZE ต้องให้ limitByte == MAX_PAYLOAD_SIZE)
pair<char*, int> resemble_legacy(vector<Segment*>& segments) {
    int part = (int)segments.size();
    int totalLength = part * MAX_PAYLOAD_SIZE;
    totalLength -= MAX_PAYLOAD_SIZE - (segments[part-1]->header.length - HEADER_SIZE);
    char* payload = new char[totalLength];
    char* curIdx = payload;
    for (int p = 0; p < part; ++p) {
        int curSize = segments[p]->header.length - HEADER_SIZE;
        memcpy(curIdx, segments[p]->payload, curSize);
        curIdx += curSize;
    }
    return {payload, totalLength};
}

// ทางเลือกที่ปลอดภัยกว่า: รวมขนาดจริงจากทุกชิ้น (ไม่ผูกกับ MAX_PAYLOAD_SIZE)
pair<char*, int> resemble_strict(vector<Segment*>& segments) {
    long long total = 0;
    for (auto* seg : segments) total += (seg->header.length - HEADER_SIZE);
    char* payload = new char[total];
    char* cur = payload;
    for (auto* seg : segments) {
        int curSize = seg->header.length - HEADER_SIZE;
        memcpy(cur, seg->payload, curSize);
        cur += curSize;
    }
    return {payload, (int)total};
}
