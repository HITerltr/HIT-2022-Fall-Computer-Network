#include <stdlib.h>
#include <WinSock2.h>
#include <time.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
using namespace std;
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")
#define SERVER_PORT 12340 //�������ݵĶ˿ں�
#define SERVER_IP "127.0.0.1" // �������� IP ��ַ
const int BUFFER_LENGTH = 1026;
const int SEND_WIND_SIZE = 5;//���ʹ��ڴ�СΪ 10��GBN ��Ӧ���� W + 1 <= N��W Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����
const int SEQ_SIZE = 20;//���ն����кŸ�����Ϊ 1~20

#define DATA_SIZE 1024

bool ack[SEQ_SIZE];

/****************************************************************/
/* -time �ӷ������˻�ȡ��ǰʱ��
-quit �˳��ͻ���
-testsr [X] ���� GBN Э��ʵ�ֿɿ����ݴ���
[X] [0,1] ģ�����ݰ���ʧ�ĸ���
[Y] [0,1] ģ�� ACK ��ʧ�ĸ���
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
// Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻� FALSE
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
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll
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
	//���ջ�����
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//Ϊ�˲���������������ӣ�����ʹ�� -time ����ӷ������˻�õ�ǰʱ��
	//ʹ�� -testsr [X] [Y] ���� GBN ����[X]��ʾ���ݰ���ʧ����
	// [Y]��ʾ ACK ��������
	printTips();
	int ret;//�ܵ����ݴ�С
	int interval = 1;//�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ�������� ack��0 ���߸�������ʾ���еĶ������� ack
	char cmd[128];
	float packetLossRatio = 0.2f; //Ĭ�ϰ���ʧ�� 0.2
	float ackLossRatio = 0.2f; //Ĭ�� ACK ��ʧ�� 0.2
	//��ʱ����Ϊ������ӣ�����ѭ����������
	srand((unsigned)time(NULL));
	std::ofstream out;
	out.open("client_out.txt");
	char cache[SEND_WIND_SIZE][DATA_SIZE];//���棬��ʱ����ʧ��δȷ�ϵķ���
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
			unsigned char u_code;//״̬��
			unsigned short seq;//�������к�
			unsigned short recvSeq;//���մ��ڴ�СΪ 1����ȷ�ϵ����к�
			unsigned short waitSeq;//�ȴ������к�
			sendto(socketClient, "-testsr", strlen("-testsr") + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			while (true)
			{
				//�ȴ� server �ظ����� UDP Ϊ����ģʽ
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
				switch (stage) {
				case 0://�ȴ����ֽ׶�
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
				case 1://�ȴ��������ݽ׶�
					/*for (int i = 0; i < SEND_WIND_SIZE; i++) {
						cout << i << ":" << ack[i] << endl;
					}*/
					if ((unsigned char)buffer[0] == 204) {
						printf("\nReceive finished\n");
						break;
					}
					seq = (unsigned short)buffer[0];
					//�����ģ����Ƿ�ʧ
					b = lossInLossRatio(packetLossRatio);
					if (b) {
						printf("The packet with a seq of %d loss\n", seq);
						continue;
					}
					printf("recv a packet with a seq of %d\n", seq);
					//������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
					//cout << "waitSeq��" << ":" << waitSeq << endl;
					if (waitSeq == seq) {
						waitSeq++;
						if (waitSeq == 21) {
							waitSeq = 1;
						}
						//�������
						printf_s("%s\n", &buffer[1]);
						//��ǰ���շ���ֱ��д���ļ�
						out.write(&buffer[1], DATA_SIZE);
						//�鿴�Ƿ���ʧ�������Ҫд���ļ�
						for (int i = waitSeq - 1; i < waitSeq - 1 + SEND_WIND_SIZE; i++) {
							i %= SEQ_SIZE;
							if (ack[i]) {
								ack[i] = false;
								//cout <<"�޸ģ�"<< i << ":" << ack[i] << endl;
								waitSeq++;
								if (waitSeq == 21) {
									waitSeq = 1;
								}
								//�ӻ���д��
								out.write(cache[i], DATA_SIZE);
							}
							else {
								break;
							}
						}
						buffer[0] = seq;//�ظ���һ����Ҫ��������к�
						recvSeq = seq;//��ǰ��ȷ�ϵ����к�Ϊ��һ����Ҫ�����к�ǰһ��
						buffer[1] = '\0';
					}
					else if (seq > waitSeq) {//����ʧ�򵽴�
						memcpy(cache[seq - 1], &buffer[1], DATA_SIZE);//�����յ������ݣ����޸���һ����Ҫ�ķ������к�
						char copy[DATA_SIZE];
						memcpy(copy, cache[seq - 1], DATA_SIZE);
						printf("���棺%s\n", copy);
						buffer[0] = seq;
						buffer[1] = '\0';
						ack[seq - 1] = true;
					}
					else {//ack��ʧ�ش���ֱ�ӷ���ack��������
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
	//�ر��׽���
	closesocket(socketClient);
	WSACleanup();
	return 0;
}