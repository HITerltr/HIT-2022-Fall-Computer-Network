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

// 构造路由表结构体
struct routingTable
{
	unsigned int dstIP;	// 目的IP
	unsigned int mask; // 掩码
	unsigned int masklen; // 掩码长度
	unsigned int nexthop; // 下一跳
};

// 创建路由表实例
vector<routingTable> routing_table; // 路由表

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
	rt.masklen = ntohl(proute->masklen); //将一个无符号长整形数从网络字节顺序转换为主机字节顺序
	rt.nexthop = ntohl(proute->nexthop);
	routing_table.push_back(rt);
	return;
}

int stud_fwd_deal(char *pBuffer, int length)
{
	int errorType = 0; // 错误编号
	int ttl = pBuffer[8]; // TTL
	int headerLength = pBuffer[0] & 0xF; // 数据报头部长度
	int dstIP = ntohl(*(unsigned int *)(pBuffer + 16)); // 目的IP地址
	if (dstIP == getIpv4Address()) // 判断分组地址与本机地址是否相同，若相同，则直接交付报文
	{
		fwd_LocalRcv(pBuffer, length); // 向上层协议交付IP分组
		return 0;
	}
	if (ttl <= 0) // 若 TTL < 0，则将该分组丢弃
	{
		errorType = STUD_FORWARD_TEST_TTLERROR;
		fwd_DiscardPkt(pBuffer, errorType);
		return 1;
	}

	// 进行路由查找
	bool match = false;	// 是否完成匹配
	unsigned int maxLen = 0; // 最长前缀匹配的长度
	int longestNum = 0;	// 最长前缀匹配的序号
	// 判断是否存在匹配
	for (int i = 0; i < routing_table.size(); i++)
	{
		if (routing_table[i].masklen > maxLen && routing_table[i].dstIP == (dstIP & routing_table[i].mask)) // 按照最长前缀原则匹配到下一跳，记录相关数据
		{
			match = true;
			longestNum = i;
			maxLen = routing_table[i].masklen;
		}
	}

	if (match) // 匹配成功，发送至下一跳
	{
		int sum = 0;
		unsigned short int newCheckSum = 0;
		char *buffer = new char[length];
		memmove(buffer, pBuffer, length);
		buffer[8]--; // 让TTL - 1
		for (int j = 1; j < 2 * headerLength + 1; j++)
		{
			if (j != 6)
			{
				sum += (buffer[(j - 1) * 2] << 8) + (buffer[(j - 1) * 2 + 1]);
				sum %= 65535;
			}
		}
		// 重新计算checksum
		newCheckSum = htons(~(unsigned short int)sum);
		memmove(buffer + 10, &newCheckSum, sizeof(unsigned short));
		// 向下一层协议发送，将数据传送至下一跳的路由
		fwd_SendtoLower(buffer, length, routing_table[longestNum].nexthop);
		return 0;
	}
	else // 若匹配失败，则进行错误处理
	{
		errorType = STUD_FORWARD_TEST_NOROUTE;
		fwd_DiscardPkt(pBuffer, errorType);
		return 1;
	}
}