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
│   └── file.txt
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

![alt text](image-5.png)
- ต้องมี folder ชื่อ **_clientFiles_** สำหรับ client เพื่อเก็บไฟล์

![alt text](image-6.png)

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


## 📜 ตัวอย่างภาพจำลองการทำงาน (Mock-up)
ภาพแสดงการสื่อสารระหว่าง Client และ Server
โดยมีการส่งไฟล์ผ่าน Socket และตรวจสอบข้อมูลด้วย Checksum การรันโปรแกรม

1. เปิดฝั่ง Server เริ่มต้นรับการเชื่อมต่อ
   $ ./server.out 8080 0 0

![alt text](image-14.png)

แสดงข้อความรอการเชื่อมต่อจาก client และ
บันทึก log ลงใน server.log

![alt text](image-17.png)

2. เปิดฝั่ง Client ขอไฟล์ (อีก terminal หนึ่ง)

   $ ./client.out 8080 0 0 file.txt

(เลือกไฟล์จากโฟลเดอร์ Files/ (กรณีส่งไฟล์ที่มีอยู่))

![alt text](image-18.png)

![alt text](image-15.png)

บันทึก log ลงใน client.log

![alt text](image-19.png)

3. Server ตรวจไฟล์ว่ามีอยู่จริง → ส่ง Metadata RESPONSE 

ตอบกลับสถานะให้ client ทราบ

![alt text](image-13.png)
 
4. Client รับ RESPONSE แล้วส่ง ACK กลับ

![alt text](image-20.png)

5. Server ส่งข้อมูลไฟล์ data segment

![alt text](image-22.png)

6. Client รับ segment เก็บบัฟเฟอร์ แล้วส่ง ACK

![alt text](image-23.png) 

7. Server ส่งสัญญาณจบ (COMPLETE) 

![alt text](image-24.png)

8. Client รับ COMPLETE แล้วส่ง ACK  → รวมบัฟเฟอร์และบันทึกเป็น clientFiles/file.txt

![alt text](image-25.png)

![alt text](image-27.png)

![alt text](image-28.png)

