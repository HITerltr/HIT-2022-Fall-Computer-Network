#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <iostream>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
#define SERVER_PORT 12340 //端口号
#define SERVER_IP "0.0.0.0" //IP 地址

#define DATA_SIZE 1024

const int BUFFER_LENGTH = 1026; //缓冲区大小，（以太网中 UDP 的数据帧中包长度应小于 1480 字节）
const int SEND_WIND_SIZE = 5;//发送窗口大小为 10，GBN 中应满足 W + 1 <= N（W 为发送窗口大小，N 为序列号个数）
//本例取序列号 0...19 共 20 个
//如果将窗口大小设为 1，则为停-等协议
const int SEQ_SIZE = 20; //序列号的个数，从 0~19 共计 20 个
//由于发送数据第一个字节如果值为 0，则数据会发送失败
//因此接收端序列号为 1~20，与发送端一一对应
BOOL ack[SEQ_SIZE];//收到 ack 情况，对应 0~19 的 ack

int counter[SEQ_SIZE];//计时器，当为负数时表示未启动，为正数时表示启动；其他同GBN

int curSeq;//当前数据包的 seq
int curAck;//当前等待确认的 ack
int totalSeq;//收到的包的总数
int totalPacket;//需要发送的包总数
//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: 获取当前系统时间，结果存入 ptime 中
// Parameter: char * ptime
//************************************
void getCurTime(char* ptime) {
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm* p;
	time(&c_time);
	p = localtime(&c_time);
	sprintf_s(buffer, "%d/%d/%d %d:%d:%d",
		p->tm_year + 1900,
		p->tm_mon,
		p->tm_mday,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	strcpy_s(ptime, sizeof(buffer), buffer);
}
//************************************
// Method: seqIsAvailable
// FullName: seqIsAvailable
// Access: public 
// Returns: bool
// Qualifier: 当前序列号 curSeq 是否可用
//************************************
bool seqIsAvailable() {
	int step;
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//序列号是否在当前发送窗口之内
	if (step >= SEND_WIND_SIZE) {
		return false;
	}
	if (ack[curSeq]) {
		return true;
	}
	return false;
}

//************************************
// Method: ackHandler
// FullName: ackHandler
// Access: public 
// Returns: void
// Qualifier: 收到 ack，累积确认，取数据帧的第一个字节
//由于发送数据时，第一个字节（序列号）为 0（ASCII）时发送失败，因此加一了，此处需要减一还原
// Parameter: char c
//************************************
void ackHandler(char c) {
	unsigned char index = (unsigned char)c - 1; //序列号减一
	printf("Recv a ack of %d\n", index + 1);
	if (curAck != index) {//分组失序，暂时缓存
		ack[index] = TRUE;
	}
	else {
		//一次分组到达，窗口向前移动（可能不止一次移动）
		ack[index] = TRUE;
		for (int i = index; i < index + SEQ_SIZE; i++) {
			i %= SEQ_SIZE;
			if (ack[i]) {
				counter[i] = -1;//计时器关闭
			}
			else {
				curAck = i + 1;//修改curAck
				break;
			}
		}
	}
}
//************************************
// Method: click
// FullName: click
// Access: public 
// Returns: void
// Qualifier: 所有开始的计时器加一
// Parameter: void
//************************************
void click() {
	for (int i = 0; i < SEQ_SIZE; i++) {
		if (counter[i] >= 0) {
			counter[i] += 1;
		}
	}
}
//************************************
// Method: checkTimeout
// FullName: checkTimeout
// Access: public 
// Returns: BOOL
// Qualifier: 检查所有计时器是否有超时的，若有返回超时的计时器的下标；否则返回-1
// 若有同时出现两个计时器同时超时，则逐个处理
// Parameter: void
//************************************
int checkTimeout() {
	bool finish;
	for (int i = 0; i < SEQ_SIZE; i++) {
		if (counter[i] >= 20) {
			return i;
		}
	}
	return -1;
}
//主函数
int main(int argc, char* argv[])
{
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//设置套接字为非阻塞模式
	int iMode = 1; //1：非阻塞，0：阻塞
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
	SOCKADDR_IN addrServer; //服务器地址
	//addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//两者均可
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		printf("Could not bind the port %d for socket.Error code is % d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	SOCKADDR_IN addrClient; //客户端地址
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //数据发送接收缓冲区
	ZeroMemory(buffer, sizeof(buffer));
	//将测试数据读入内存
	std::ifstream icin;
	icin.open("server_in.txt");
	char data[DATA_SIZE * 113];//需要发送的数据
	ZeroMemory(data, sizeof(data));
	icin.read(data, DATA_SIZE * 113);
	icin.close();
	totalPacket = strlen(data) / DATA_SIZE;
	int recvSize;
	for (int i = 0; i < SEQ_SIZE; ++i) {
		ack[i] = TRUE;
		counter[i] = -1;
	}
	char cache[SEND_WIND_SIZE + 1][DATA_SIZE + 1];//缓存，暂时保存发送但未受到ack的分组
	while (true) {
		//非阻塞接收，若没有收到数据，返回值为-1
		recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
		if (recvSize < 0) {
			Sleep(200);
			continue;
		}
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0) {
			getCurTime(buffer);
		}
		else if (strcmp(buffer, "-quit") == 0) {
			strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
		}
		else if (strcmp(buffer, "-testsr") == 0) {
			//进入 gbn 测试阶段
			//首先 server（server 处于 0 状态）向 client 发送 205 状态码（server进入 1 状态）
			//server 等待 client 回复 200 状态码，如果收到（server 进入 2 状态），则开始传输文件，否则延时等待直至超时\
			//在文件传输阶段，server 发送窗口大小设为
			ZeroMemory(buffer, sizeof(buffer));
			int recvSize;
			int waitCount = 0;
			printf("Begain to test SR protocol,please don't abort the process\n");
			//加入了一个握手阶段
			//首先服务器向客户端发送一个 205 大小的状态码表示服务器准备好了，可以发送数据
			//客户端收到 205 之后回复一个 200 大小的状态码，表示客户端准备好了，可以接收数据了
			//服务器收到 200 状态码之后，就开始使用 SR 发送数据了
			printf("Shake hands stage\n");
			int stage = 0;
			bool runFlag = true;
			while (runFlag) {
				switch (stage) {
				case 0://发送 205 阶段
					buffer[0] = 205;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://等待接收 200 阶段，没有收到则计数器+1，超时则放弃此次“连接”，等待从第一步开始
					recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0) {
						++waitCount;
						if (waitCount > 20) {
							runFlag = false;
							printf("Timeout error\n");
							break;
						}
						Sleep(500);
						continue;
					}
					else {
						if ((unsigned char)buffer[0] == 200) {
							printf("Begin a file transfer\n");
							printf("File size is %dB, each packet is 1024B and packet total num is % d\n", strlen(data), totalPacket);
							curSeq = 0;
							curAck = 0;
							totalSeq = 0;
							//waitCount = 0;
							stage = 2;
						}
					}
					break;
				case 2://数据传输阶段
					if (seqIsAvailable()) {
						if (totalSeq <= totalPacket) {
							//发送给客户端的序列号从 1 开始
							buffer[0] = curSeq + 1;
							ack[curSeq] = FALSE;
							memcpy(&buffer[1], data + DATA_SIZE * totalSeq, DATA_SIZE);
							memcpy(cache[curSeq], data + DATA_SIZE * totalSeq, DATA_SIZE);//缓存分组
							printf("send a packet with a seq of %d\n", curSeq + 1);
							sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							counter[curSeq] = 0;//计时器开启
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							Sleep(500);
						}
					}
					//等待 Ack，若没有收到，则返回值为-1，计数器+1
					recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0) {
						click();
						//20 次等待 ack 则超时重传
						if (checkTimeout() != -1) {
							int index = checkTimeout();
							printf("Seq %d time out.\n", index + 1);
							buffer[0] = index + 1;
							memcpy(&buffer[1], cache[index], DATA_SIZE);
							printf("Re : send a packet with a seq of %d\n", index + 1);
							sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							counter[index] = 0;//重置计时器
						}
					}
					else {
						//收到 ack
						ackHandler(buffer[0]);
						counter[buffer[0] - 1] = -1;//计时器关闭
						//数判断是否传输完成
						if (totalSeq >= totalPacket) {//传输完成，若都收到Ack则传输结束，否则不发送数据等待超时重传
							bool finish = true;
							for (int i = 0; i < SEQ_SIZE; i++) {
								if (!ack[i]) {
									finish = false;
									break;
								}
							}
							if (finish) {
								printf("\nServer send finish!\n");
								buffer[0] = 204;
								sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
								Sleep(100);
								runFlag = false;
								break;
							}
						}
					}
					Sleep(500);
					break;
				}
			}
		}
		sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
		Sleep(500);
	}
	//关闭套接字，卸载库
	closesocket(sockServer);
	WSACleanup();
	return 0;
}