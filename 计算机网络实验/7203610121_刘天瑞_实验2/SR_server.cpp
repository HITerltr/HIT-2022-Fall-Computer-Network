#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <iostream>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
#define SERVER_PORT 12340 //�˿ں�
#define SERVER_IP "0.0.0.0" //IP ��ַ

#define DATA_SIZE 1024

const int BUFFER_LENGTH = 1026; //��������С������̫���� UDP ������֡�а�����ӦС�� 1480 �ֽڣ�
const int SEND_WIND_SIZE = 5;//���ʹ��ڴ�СΪ 10��GBN ��Ӧ���� W + 1 <= N��W Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����
//����ȡ���к� 0...19 �� 20 ��
//��������ڴ�С��Ϊ 1����Ϊͣ-��Э��
const int SEQ_SIZE = 20; //���кŵĸ������� 0~19 ���� 20 ��
//���ڷ������ݵ�һ���ֽ����ֵΪ 0�������ݻᷢ��ʧ��
//��˽��ն����к�Ϊ 1~20���뷢�Ͷ�һһ��Ӧ
BOOL ack[SEQ_SIZE];//�յ� ack �������Ӧ 0~19 �� ack

int counter[SEQ_SIZE];//��ʱ������Ϊ����ʱ��ʾδ������Ϊ����ʱ��ʾ����������ͬGBN

int curSeq;//��ǰ���ݰ��� seq
int curAck;//��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;//�յ��İ�������
int totalPacket;//��Ҫ���͵İ�����
//************************************
// Method: getCurTime
// FullName: getCurTime
// Access: public
// Returns: void
// Qualifier: ��ȡ��ǰϵͳʱ�䣬������� ptime ��
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
// Qualifier: ��ǰ���к� curSeq �Ƿ����
//************************************
bool seqIsAvailable() {
	int step;
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//���к��Ƿ��ڵ�ǰ���ʹ���֮��
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
// Qualifier: �յ� ack���ۻ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ�
//���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0��ASCII��ʱ����ʧ�ܣ���˼�һ�ˣ��˴���Ҫ��һ��ԭ
// Parameter: char c
//************************************
void ackHandler(char c) {
	unsigned char index = (unsigned char)c - 1; //���кż�һ
	printf("Recv a ack of %d\n", index + 1);
	if (curAck != index) {//����ʧ����ʱ����
		ack[index] = TRUE;
	}
	else {
		//һ�η��鵽�������ǰ�ƶ������ܲ�ֹһ���ƶ���
		ack[index] = TRUE;
		for (int i = index; i < index + SEQ_SIZE; i++) {
			i %= SEQ_SIZE;
			if (ack[i]) {
				counter[i] = -1;//��ʱ���ر�
			}
			else {
				curAck = i + 1;//�޸�curAck
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
// Qualifier: ���п�ʼ�ļ�ʱ����һ
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
// Qualifier: ������м�ʱ���Ƿ��г�ʱ�ģ����з��س�ʱ�ļ�ʱ�����±ꣻ���򷵻�-1
// ����ͬʱ����������ʱ��ͬʱ��ʱ�����������
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
//������
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
	//�����׽���Ϊ������ģʽ
	int iMode = 1; //1����������0������
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//����������
	SOCKADDR_IN addrServer; //��������ַ
	//addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//���߾���
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		printf("Could not bind the port %d for socket.Error code is % d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	SOCKADDR_IN addrClient; //�ͻ��˵�ַ
	int length = sizeof(SOCKADDR);
	char buffer[BUFFER_LENGTH]; //���ݷ��ͽ��ջ�����
	ZeroMemory(buffer, sizeof(buffer));
	//���������ݶ����ڴ�
	std::ifstream icin;
	icin.open("server_in.txt");
	char data[DATA_SIZE * 113];//��Ҫ���͵�����
	ZeroMemory(data, sizeof(data));
	icin.read(data, DATA_SIZE * 113);
	icin.close();
	totalPacket = strlen(data) / DATA_SIZE;
	int recvSize;
	for (int i = 0; i < SEQ_SIZE; ++i) {
		ack[i] = TRUE;
		counter[i] = -1;
	}
	char cache[SEND_WIND_SIZE + 1][DATA_SIZE + 1];//���棬��ʱ���淢�͵�δ�ܵ�ack�ķ���
	while (true) {
		//���������գ���û���յ����ݣ�����ֵΪ-1
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
			//���� gbn ���Խ׶�
			//���� server��server ���� 0 ״̬���� client ���� 205 ״̬�루server���� 1 ״̬��
			//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�ֱ����ʱ\
			//���ļ�����׶Σ�server ���ʹ��ڴ�С��Ϊ
			ZeroMemory(buffer, sizeof(buffer));
			int recvSize;
			int waitCount = 0;
			printf("Begain to test SR protocol,please don't abort the process\n");
			//������һ�����ֽ׶�
			//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬���ʾ������׼�����ˣ����Է�������
			//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
			//�������յ� 200 ״̬��֮�󣬾Ϳ�ʼʹ�� SR ����������
			printf("Shake hands stage\n");
			int stage = 0;
			bool runFlag = true;
			while (runFlag) {
				switch (stage) {
				case 0://���� 205 �׶�
					buffer[0] = 205;
					sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
					Sleep(100);
					stage = 1;
					break;
				case 1://�ȴ����� 200 �׶Σ�û���յ��������+1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
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
				case 2://���ݴ���׶�
					if (seqIsAvailable()) {
						if (totalSeq <= totalPacket) {
							//���͸��ͻ��˵����кŴ� 1 ��ʼ
							buffer[0] = curSeq + 1;
							ack[curSeq] = FALSE;
							memcpy(&buffer[1], data + DATA_SIZE * totalSeq, DATA_SIZE);
							memcpy(cache[curSeq], data + DATA_SIZE * totalSeq, DATA_SIZE);//�������
							printf("send a packet with a seq of %d\n", curSeq + 1);
							sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							counter[curSeq] = 0;//��ʱ������
							++curSeq;
							curSeq %= SEQ_SIZE;
							++totalSeq;
							Sleep(500);
						}
					}
					//�ȴ� Ack����û���յ����򷵻�ֵΪ-1��������+1
					recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
					if (recvSize < 0) {
						click();
						//20 �εȴ� ack ��ʱ�ش�
						if (checkTimeout() != -1) {
							int index = checkTimeout();
							printf("Seq %d time out.\n", index + 1);
							buffer[0] = index + 1;
							memcpy(&buffer[1], cache[index], DATA_SIZE);
							printf("Re : send a packet with a seq of %d\n", index + 1);
							sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
							counter[index] = 0;//���ü�ʱ��
						}
					}
					else {
						//�յ� ack
						ackHandler(buffer[0]);
						counter[buffer[0] - 1] = -1;//��ʱ���ر�
						//���ж��Ƿ������
						if (totalSeq >= totalPacket) {//������ɣ������յ�Ack������������򲻷������ݵȴ���ʱ�ش�
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
	//�ر��׽��֣�ж�ؿ�
	closesocket(sockServer);
	WSACleanup();
	return 0;
}