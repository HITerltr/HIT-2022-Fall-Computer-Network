/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <vector>
#include <iostream>
using std::cout;
using std::vector;
// system support
extern void fwd_LocalRcv(char *pBuffer, int length);
extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);
extern void fwd_DiscardPkt(char *pBuffer, int type);
extern unsigned int getIpv4Address();
// implemented by students

// ����·�ɱ�ṹ��
struct routingTable
{
	unsigned int dstIP;	// Ŀ��IP
	unsigned int mask; // ����
	unsigned int masklen; // ���볤��
	unsigned int nexthop; // ��һ��
};

// ����·�ɱ�ʵ��
vector<routingTable> routing_table; // ·�ɱ�

void stud_Route_Init()
{
	routing_table.clear();
	return;
}

void stud_route_add(stud_route_msg *proute)
{
	routingTable rt;
	rt.dstIP = ntohl(proute->dest);
	rt.mask = (1 << 31) >> (ntohl(proute->masklen) - 1);
	rt.masklen = ntohl(proute->masklen); //��һ���޷��ų��������������ֽ�˳��ת��Ϊ�����ֽ�˳��
	rt.nexthop = ntohl(proute->nexthop);
	routing_table.push_back(rt);
	return;
}

int stud_fwd_deal(char *pBuffer, int length)
{
	int errorType = 0; // ������
	int ttl = pBuffer[8]; // TTL
	int headerLength = pBuffer[0] & 0xF; // ���ݱ�ͷ������
	int dstIP = ntohl(*(unsigned int *)(pBuffer + 16)); // Ŀ��IP��ַ
	if (dstIP == getIpv4Address()) // �жϷ����ַ�뱾����ַ�Ƿ���ͬ������ͬ����ֱ�ӽ�������
	{
		fwd_LocalRcv(pBuffer, length); // ���ϲ�Э�齻��IP����
		return 0;
	}
	if (ttl <= 0) // �� TTL < 0���򽫸÷��鶪��
	{
		errorType = STUD_FORWARD_TEST_TTLERROR;
		fwd_DiscardPkt(pBuffer, errorType);
		return 1;
	}

	// ����·�ɲ���
	bool match = false;	// �Ƿ����ƥ��
	unsigned int maxLen = 0; // �ǰ׺ƥ��ĳ���
	int longestNum = 0;	// �ǰ׺ƥ������
	// �ж��Ƿ����ƥ��
	for (int i = 0; i < routing_table.size(); i++)
	{
		if (routing_table[i].masklen > maxLen && routing_table[i].dstIP == (dstIP & routing_table[i].mask)) // �����ǰ׺ԭ��ƥ�䵽��һ������¼�������
		{
			match = true;
			longestNum = i;
			maxLen = routing_table[i].masklen;
		}
	}

	if (match) // ƥ��ɹ�����������һ��
	{
		int sum = 0;
		unsigned short int newCheckSum = 0;
		char *buffer = new char[length];
		memmove(buffer, pBuffer, length);
		buffer[8]--; // ��TTL - 1
		for (int j = 1; j < 2 * headerLength + 1; j++)
		{
			if (j != 6)
			{
				sum += (buffer[(j - 1) * 2] << 8) + (buffer[(j - 1) * 2 + 1]);
				sum %= 65535;
			}
		}
		// ���¼���checksum
		newCheckSum = htons(~(unsigned short int)sum);
		memmove(buffer + 10, &newCheckSum, sizeof(unsigned short));
		// ����һ��Э�鷢�ͣ������ݴ�������һ����·��
		fwd_SendtoLower(buffer, length, routing_table[longestNum].nexthop);
		return 0;
	}
	else // ��ƥ��ʧ�ܣ�����д�����
	{
		errorType = STUD_FORWARD_TEST_NOROUTE;
		fwd_DiscardPkt(pBuffer, errorType);
		return 1;
	}
}