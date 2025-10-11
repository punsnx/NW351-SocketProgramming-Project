# NW351-SocketProgramming-Project

# 🛰️ ระบบรับ–ส่งไฟล์ผ่านเครือข่าย (Socket Programming Project)

โปรเจกต์นี้เป็นการจำลองการสื่อสารระหว่าง **Client** และ **Server** ผ่านเครือข่าย โดยใช้ **UDP Socket Programming (C++)**  
ซึ่งสามารถส่งไฟล์, ตรวจสอบ checksum, แบ่งไฟล์ด้วย packetization, บันทึก log และจัดการไฟล์ที่ฝั่ง server-client ได้ ตามหลักการ Reliable Data Transfer

---

## 📁 โครงสร้างไฟล์ของโปรเจกต์

```
pun
├── checksum.cc
├── client.cc
├── clientFiles
│   └── history.txt
├── client.log
├── client.out
├── file-handler.cc
├── Files
│   ├── cap.mp4
│   ├── file2.txt
│   ├── file3.txt
│   ├── file4.txt
│   ├── file.txt
│   ├── hello.txt
│   ├── history.txt
│   └── img1.jpg
├── header
│   ├── checksum.h
│   ├── client.h
│   ├── file-handler.h
│   ├── packetize.h
│   ├── project-header.h
│   ├── segment.h
│   └── simulate.h
├── main.cc
├── Makefile
├── mock.cc
├── packetize.cc
├── server.log
├── server-newlife.cc
└── server.out
```

---

## ⚙️ ขั้นตอนการคอมไพล์และรันโปรแกรม

### 🔧 การคอมไพล์

ใช้คำสั่ง `make` จากไฟล์ `Makefile` เพื่อคอมไพล์โปรแกรมทั้งหมด

```bash
$ make all

จะได้ไฟล์ binary ดังนี้

client.out (ฝั่ง Client)

server.out (ฝั่ง Server)
```

```bash
$ make client
เพื่อ compile เฉพาะ ฝั่ง client โดยจะได้ไฟล์ client.out
```

```bash
$ make server
เพื่อ compile เฉพาะ ฝั่ง server โดยจะได้ไฟล์ server.out
```

```bash
$ make clean
เพื่อลบไฟล์จากการ compile ทั้งหมด
```

### 🔧 การสร้าง folder เก็บไฟล์

สร้างโฟลเดอร์ดังต่อไปนี้ใน root directory ของโปรเจก์ (ตรงกับตำแหน่งที่เก็บ binary file ของแต่ละฝั่ง server และ client) ดูได้จากโครงสร้างของโปรเจกต์

- ต้องมี folder ชื่อ **_files_** สำหรับ server เพื่อเก็บไฟล์
- ต้องมี folder ชื่อ **_clientFiles_** สำหรับ client เพื่อเก็บไฟล์

### 🔧 การรันโปรแกรมฝั่ง Server

```bash
./server.out <port> <drop_percentage> <corrupt_percentage>
```

**_NOTE_**

```
1. <port> เป็นหมายเลขของ port ที่ใช้ในการ run UDP socket ของ server
2. <drop_percentage> เป็นร้อยละข้อง packet เพื่อจำลองการ drop ใน server หากไม่ใส่จะมีต่าเริ่มต้นเป็น 0
3. <corrupt_percentage> เป็นร้อยละข้อง packet เพื่อจำลองการ corrupt ใน server หากไม่ใส่จะมีต่าเริ่มต้นเป็น 0
```

### 🔧 การรันโปรแกรมฝั่ง Client

```bash
./client.out <ip_address> <port> <drop_percentage> <corrupt_percentage> <files>
```

**_NOTE_**

```
1. <ip_address> เป็นหมายเลขของ server ip address ที่ใช้ในการเชื่อมต่อ
2. <port> เป็นหมายเลขของ server port ที่ใช้ในการเชื่อมต่อ
3. <drop_percentage> เป็นร้อยละข้อง packet เพื่อจำลองการ drop ใน server
4. <corrupt_percentage> เป็นร้อยละข้อง packet เพื่อจำลองการ corrupt ใน server
5. <files> เป็นชื่อไฟล์ที่ต้องการจากร้องขอจาก server (มีได้มากกว่า 1 ไฟล์ ขั้นด้วยช่องว่าง)
```

## 🚀ตัวอย่างภาพจำลอง (Mock-up)

ภาพแสดงการสื่อสารระหว่าง Client และ Server
โดยมีการส่งไฟล์ผ่าน Socket และตรวจสอบข้อมูลด้วย Checksum�ารรันโปรแกรม

1. เปิดฝั่ง Server
   $ ./server.out

รอให้ server เริ่มฟัง (listen) การเชื่อมต่อจาก client

2. เปิดฝั่ง Client (อีก terminal หนึ่ง)
   $ ./client.out

เมื่ออทำตามขั้นตอนสำเร็จ จะสามารถเลือกส่งไฟล์/ดาวน์โหลดไฟล์ได้ตามคำสั่งในโปรแกรม

## 📜 ตัวอย่างการทำงาน

Server เริ่มต้นรับการเชื่อมต่อ

แสดงข้อความรอ client

บันทึก log ลงใน server.log

Client ขอส่งไฟล์

เลือกไฟล์จากโฟลเดอร์ clientFiles/

โปรแกรมจะคำนวณ checksum เพื่อยืนยันความถูกต้อง

บันทึก log ลงใน client.log

Server รับไฟล์

ตรวจสอบ checksum

บันทึกไฟล์ในโฟลเดอร์ Files/

ตอบกลับสถานะให้ client ทราบ

## 🔍 ตัวอย่างภาพจำลอง (Mock-up)

ภาพแสดงการสื่อสารระหว่าง Client และ Server
โดยมีการส่งไฟล์ผ่าน Socket และตรวจสอบข้อมูลด้วย Checksum
