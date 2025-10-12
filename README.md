# NW351-SocketProgramming-Project

# 🛰️ ระบบรับ–ส่งไฟล์ผ่านเครือข่าย (Socket Programming Project)

โปรเจกต์นี้เป็นการจำลองการสื่อสารระหว่าง **Client** และ **Server** ผ่านเครือข่าย โดยใช้ **UDP Socket Programming (C++)**  
ซึ่งสามารถส่งไฟล์, ตรวจสอบ checksum, แบ่งไฟล์ด้วย packetization, บันทึก log และจัดการไฟล์ที่ฝั่ง server-client ได้ ตามหลักการ Reliable Data Transfer

---

## 📁 โครงสร้างไฟล์ของโปรเจกต์

```
.
├── Files
│   ├── cap.mp4
│   ├── cat.txt
│   ├── ckwVibe.mp4
│   ├── file.txt
│   ├── file2.txt
│   ├── file3.txt
│   ├── file4.txt
│   ├── hello.txt
│   ├── history.txt
│   └── img1.jpg
├── Makefile
├── README.md
├── TEST_PARAMS.txt
├── checksum.cc
├── client.cc
├── file-handler.cc
├── header
│   ├── checksum.h
│   ├── client.h
│   ├── file-handler.h
│   ├── packetize.h
│   ├── project-header.h
│   ├── segment.h
│   └── simulate.h
├── packetize.cc
├── release-test-logs
│   └── release-test-logs.rar
├── server.cc
└── test.sh
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

![alt text](/images/image-5.png)
- ต้องมี folder ชื่อ **_clientFiles_** สำหรับ client เพื่อเก็บไฟล์

![alt text](/images/image-6.png)

## 📜 ตัวอย่างไฟล์ในการทดสอบ
ผู้ทดสอบสามาถเลือกไฟล์สำหรับการทดสอบในโฟลเดอร์ Files โดยมีไฟล์ดังนี้
```
- cap.mp4
- ckwVibe.mp4
- file2.txt
- file4.txt
- history.txt
- cat.txt
- file.txt
- file3.txt
- hello.txt
- img1.jpg
```

หมายเหตุ ผู้ทดสอบสามารถเพิ่มไฟล์เองใน directory Files ได้เช่นกัน

![alt text](/images/image-29.png)

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
**_EXAMPLE_**

`./server.out 8080 0 0` หรือ `./server.out 8080 0 0 > server.log` หากต้องการเก็บผลลัพธ์ลงในไฟล์ `server.log`

### 🔧 การรันโปรแกรมฝั่ง Client

```bash
./client.out <ip_address> <port> <drop_percentage> <corrupt_percentage> <files>
```

**_NOTE_**

```
1. <ip_address> เป็นหมายเลขของ server ip address ที่ใช้ในการเชื่อมต่อ
2. <port> เป็นหมายเลขของ server port ที่ใช้ในการเชื่อมต่อ
3. <drop_percentage> เป็นร้อยละข้อง Drop Percentage เพื่อจำลองการ drop ใน server เป็นจำนวนเต็ม
4. <corrupt_percentage> เป็นร้อยละของ Corrupt Percentage เพื่อจำลองการ corrupt ใน server เป็นจำนวนเต็ม
5. <files> เป็นชื่อไฟล์ที่ต้องการจากร้องขอจาก server (มีได้มากกว่า 1 ไฟล์ ขั้นด้วยช่องว่าง)
```
**_EXAMPLE_**

`./client.out 127.0.0.1 8080 5 5 file1.txt file2.txt` 
หรือ `./client.out 127.0.0.1 8080 5 5 file1.txt file2.txt > client.log` หากต้องการเก็บผลลัพธ์ลงในไฟล์ `client.log`

## 💻 ตัวอย่างภาพจำลองการทำงาน (Mock-up)
ภาพแสดงการสื่อสารระหว่าง Client และ Server
โดยมีการส่งไฟล์ผ่าน Socket และตรวจสอบข้อมูลด้วย Checksum การรันโปรแกรม

### 1. Compile
`$ make clean` ตามด้วย `$ make all`

![alt text](/images/image-30.png)


### 2. เปิด Terminal ฝั่ง Server เริ่มต้นรับการเชื่อมต่อ 
`./server.out 8080 0 0` หรือ `./server.out 8080 0 0 > server.log` 


แสดงข้อความรอการเชื่อมต่อจาก client และ
บันทึก log ลงใน server.log

![alt text](/images/image-17.png)

### 3. เปิด Terminal ฝั่ง Client ขอไฟล์ (อีก terminal หนึ่ง)
`./client.out 127.0.0.1 8080 5 5 file1.txt file2.txt` 
หรือ `./client.out 127.0.0.1 8080 5 5 file1.txt file2.txt > client.log` 

(เลือกไฟล์จากโฟลเดอร์ Files/ (กรณีส่งไฟล์ที่มีอยู่))


![alt text](/images/image-15.png)

บันทึก log ลงใน client.log

![alt text](/images/image-19.png)

ตัวอย่างการเปิด 2 terminal
![alt text](/images/image-31.png)


### 4. Server ตรวจไฟล์ว่ามีอยู่จริง → ส่ง Metadata RESPONSE 

ตอบกลับสถานะให้ client ทราบ

![alt text](/images/image-13.png)
 
### 5. Client รับ RESPONSE แล้วส่ง ACK กลับ

![alt text](/images/image-20.png)

### 6. Server ส่งข้อมูลไฟล์ data segment

![alt text](/images/image-22.png)

### 7. Client รับ segment เก็บลงใน vecter แล้วส่ง ACK

![alt text](/images/image-23.png) 

### 8. Server ส่งสัญญาณจบ (COMPLETE) 

![alt text](/images/image-24.png)

### 9. Client รับ COMPLETE แล้วส่ง ACK  → รวม segments และบันทึกใน clientFiles/file.txt

![alt text](/images/image-25.png)

![alt text](/images/image-27.png)

![alt text](/images/image-28.png)