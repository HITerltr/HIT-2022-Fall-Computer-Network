#include <stdlib.h>
#include <WinSock2.h>
#include <time.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
using namespace std;
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")
#define SERVER_PORT 12340 //接收数据的端口号
#define SERVER_IP "127.0.0.1" // 服务器的 IP 地址
const int BUFFER_LENGTH = 1026;
const int SEND_WIND_SIZE = 5;//发送窗口大小为 10，GBN 中应满足 W + 1 <= N（W 为发送窗口大小，N 为序列号个数）
const int SEQ_SIZE = 20;//接收端序列号个数，为 1~20

#define DATA_SIZE 1024

bool ack[SEQ_SIZE];

/****************************************************************/
/* -time 从服务器端获取当前时间
-quit 退出客户端
-testsr [X] 测试 GBN 协议实现可靠数据传输
[X] [0,1] 模拟数据包丢失的概率
[Y] [0,1] 模拟 ACK 丢失的概率
*/
/****************************************************************/
void printTips() {
	printf(" -time to get current time \n");
	printf(" -quit to exit client \n");
	printf(" -testsr [X] [Y] to test the sr \n");
}
//************************************
// Method: lossInLossRatio
// FullName: lossInLossRatio
// Access: public 
// Returns: BOOL
// Qualifier: 根据丢失率随机生成一个数字，判断是否丢失,丢失则返回TRUE，否则返回 FALSE
// Parameter: float lossRatio [0,1]
//************************************
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		return TRUE;
	}
	return FALSE;
}
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
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//接收缓冲区
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//为了测试与服务器的连接，可以使用 -time 命令从服务器端获得当前时间
	//使用 -testsr [X] [Y] 测试 GBN 其中[X]表示数据包丢失概率
	// [Y]表示 ACK 丢包概率
	printTips();
	int ret;//受到数据大小
	int interval = 1;//收到数据包之后返回 ack 的间隔，默认为 1 表示每个都返回 ack，0 或者负数均表示所有的都不返回 ack
	char cmd[128];
	float packetLossRatio = 0.2f; //默认包丢失率 0.2
	float ackLossRatio = 0.2f; //默认 ACK 丢失率 0.2
	//用时间作为随机种子，放在循环的最外面
	srand((unsigned)time(NULL));
	std::ofstream out;
	out.open("client_out.txt");
	char cache[SEND_WIND_SIZE][DATA_SIZE];//缓存，暂时保存失序但未确认的分组
	while (true) {
		gets_s(buffer);
		//printf("buffer:%s\n", buffer);
		ret = sscanf_s(buffer, "%s%f%f", &cmd, sizeof(cmd), &packetLossRatio, &ackLossRatio);
		printf("buffer:%s\n", cmd);
		printf("packet:%f2\n", packetLossRatio);
		printf("ack:%2f\n", ackLossRatio);
		if (!strcmp(cmd, "-testsr")) {
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The loss ratio of packet is %.2f,the loss ratio of ack is % .2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//状态码
			unsigned short seq;//包的序列号
			unsigned short recvSeq;//接收窗口大小为 1，已确认的序列号
			unsigned short waitSeq;//等待的序列号
			sendto(socketClient, "-testsr", strlen("-testsr") + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			while (true)
			{
				//等待 server 回复设置 UDP 为阻塞模式
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
				switch (stage) {
				case 0://等待握手阶段
					u_code = (unsigned char)buffer[0];
					if ((unsigned char)buffer[0] == 205)
					{
						printf("Ready for file transmission\n");
						buffer[0] = 200;
						buffer[1] = '\0';
						sendto(socketClient, buffer, 2, 0,
							(SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						stage = 1;
						recvSeq = 0;
						waitSeq = 1;
						for (int i = 0; i < SEQ_SIZE; i++)
						{
							ack[i] = false;
						}
					}
					break;
				case 1://等待接收数据阶段
					/*for (int i = 0; i < SEND_WIND_SIZE; i++) {
						cout << i << ":" << ack[i] << endl;
					}*/
					if ((unsigned char)buffer[0] == 204) {
						printf("\nReceive finished\n");
						break;
					}
					seq = (unsigned short)buffer[0];
					//随机法模拟包是否丢失
					b = lossInLossRatio(packetLossRatio);
					if (b) {
						printf("The packet with a seq of %d loss\n", seq);
						continue;
					}
					printf("recv a packet with a seq of %d\n", seq);
					//如果是期待的包，正确接收，正常确认即可
					//cout << "waitSeq：" << ":" << waitSeq << endl;
					if (waitSeq == seq) {
						waitSeq++;
						if (waitSeq == 21) {
							waitSeq = 1;
						}
						//输出数据
						printf_s("%s\n", &buffer[1]);
						//当前接收分组直接写入文件
						out.write(&buffer[1], DATA_SIZE);
						//查看是否有失序分组需要写入文件
						for (int i = waitSeq - 1; i < waitSeq - 1 + SEND_WIND_SIZE; i++) {
							i %= SEQ_SIZE;
							if (ack[i]) {
								ack[i] = false;
								//cout <<"修改："<< i << ":" << ack[i] << endl;
								waitSeq++;
								if (waitSeq == 21) {
									waitSeq = 1;
								}
								//从缓存写入
								out.write(cache[i], DATA_SIZE);
							}
							else {
								break;
							}
						}
						buffer[0] = seq;//回复下一个需要分组的序列号
						recvSeq = seq;//当前已确认的序列号为下一个需要的序列号前一个
						buffer[1] = '\0';
					}
					else if (seq > waitSeq) {//分组失序到达
						memcpy(cache[seq - 1], &buffer[1], DATA_SIZE);//缓存收到的数据，不修改下一个需要的分组序列号
						char copy[DATA_SIZE];
						memcpy(copy, cache[seq - 1], DATA_SIZE);
						printf("缓存：%s\n", copy);
						buffer[0] = seq;
						buffer[1] = '\0';
						ack[seq - 1] = true;
					}
					else {//ack丢失重传，直接返回ack，不缓存
						buffer[0] = seq;
						buffer[1] = '\0';
					}
					b = lossInLossRatio(ackLossRatio);
					if (b) {
						printf("The ack of %d loss\n", (unsigned char)buffer[0]);
						continue;
					}
					sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					printf("send a ack of %d\n", (unsigned char)buffer[0]);
					break;
				}
				Sleep(500);
			}
		}
		sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!")) {
			break;
		}
		printTips();
	}
	//关闭套接字
	closesocket(socketClient);
	WSACleanup();
	return 0;
}