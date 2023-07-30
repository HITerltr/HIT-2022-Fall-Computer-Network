/*
* THIS FILE IS FOR IP RECEIVE TEST
*/
#include "sysInclude.h"
#include <stdio.h>
#include <string.h>

extern void ip_DiscardPkt(char *pBuffer, int type);

extern void ip_SendtoLower(char *pBuffer, int length);

extern void ip_SendtoUp(char *pBuffer, int length);

extern unsigned int getIpv4Address();
}

int stud_ip_recv(char *pBuffer, unsigned short length)
{
  int errorType = 0;

  // 检测version错误
  int version = pBuffer[0] >> 4;
  if (version != 4)
  {
    errorType = STUD_IP_TEST_VERSION_ERROR;
    ip_DiscardPkt(pBuffer, errorType);
    return 1;
  }

  // 检测headerLength错误
  int headerLength = pBuffer[0] & 0xF;
  if (headerLength < 5)
  {
    errorType = STUD_IP_TEST_HEADLEN_ERROR;
    ip_DiscardPkt(pBuffer, errorType);
    return 1;
  }

  // 检测TTL错误
  int ttl = (unsigned short)pBuffer[8];
  if (ttl <= 0)
  {
    errorType = STUD_IP_TEST_TTL_ERROR;
    ip_DiscardPkt(pBuffer, errorType);
    return 1;
  }

  // 检测目的地址错误
  unsigned int destIP = ntohl(*(unsigned int *)(pBuffer + 16));
  unsigned int localIP = getIpv4Address();
  if (destIP != 0xFFFFFFFF && destIP != localIP)
  {
    errorType = STUD_IP_TEST_DESTINATION_ERROR;
    ip_DiscardPkt(pBuffer, errorType);
    return 1;
  }

  // 检测checkSum错误
  unsigned short sum = 0;
  unsigned short temp = 0;
  for (int i = 0; i < headerLength * 2; i++)
  {
    temp = ((unsigned char)pBuffer[i * 2] << 8) + (unsigned char)pBuffer[i * 2 + 1]; // <<8表示其做高2位
    if (sum + temp > 0xFFFF) // 若计算结果 > 0xFFFF，则将高16位加在低16位上
      sum += 1;
    sum += temp;
  }
  if (sum != 0xFFFF) // 若计算结果 ≠ FFFF，说明数据报发生错误
  {
    errorType = STUD_IP_TEST_CHECKSUM_ERROR;
    ip_DiscardPkt(pBuffer, errorType);
    return 1;
  }

  // 无错误显示，则表示成功接受，再上传给上层
  ip_SendtoUp(pBuffer, length);
  return 0;
}

int stud_ip_Upsend(char *pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr, byte protocol, byte ttl)
{
  char *IPBuffer = (char *)malloc((len + 20) * sizeof(char)); // IPBuffer的每一位对应着一个字节，20是报文头的字节数
  memset(IPBuffer, 0, len + 20);
  IPBuffer[0] = 0x45; // 构造版本号与头长度位
  unsigned short totalLength = htons(len + 20); // IPv4报文分组总长度
  memmove(IPBuffer + 2, &totalLength, 2); // 构造报文头的totalLength部分
  IPBuffer[8] = ttl; // 构造TTL
  IPBuffer[9] = protocol; // 构造协议号
  unsigned int src = htonl(srcAddr);
  unsigned int dst = htonl(dstAddr);
  memmove(IPBuffer + 12, &src, 4); // 源IP地址
  memmove(IPBuffer + 16, &dst, 4); // 目的IP地址
  unsigned short sum = 0;
  unsigned short temp = 0;
  unsigned short checkSum = 0;

  //计算checksum
  for (int i = 0; i < 10; i++)
  {
    temp = ((unsigned char)IPBuffer[i * 2] << 8) + (unsigned char)IPBuffer[i * 2 + 1]; // <<8表示其做高2位
    if (sum + temp > 0xFFFF) // 若结果>0xFFFF，则将高16位加在低16位上
      sum += 1;
    sum += temp;
  }
  checkSum = htons(0xFFFF - sum); // 取反，得到最终的checkSum
  memmove(IPBuffer + 10, &checkSum, 2); // 构造报文头的checkSum部分
  memmove(IPBuffer + 20, pBuffer, len); // 构造报文的实际内容
  ip_SendtoLower(IPBuffer, len + 20);   // 向下一层协议发送
  return 0;
}