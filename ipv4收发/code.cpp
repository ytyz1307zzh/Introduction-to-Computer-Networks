
#include "sysInclude.h"
#include <iostream>
#include <winsock.h>
#include <cstring>
using namespace std;

extern void ip_DiscardPkt(char* pBuffer, int type);

extern void ip_SendtoLower(char*pBuffer, int length);

extern void ip_SendtoUp(char *pBuffer, int length);

extern unsigned int getIpv4Address();

// implemented by students

typedef struct ipv4
{
	unsigned short version_len_service;
	unsigned short total_length;
	unsigned short tag;
	unsigned short flag_offset;
	unsigned short ttl_protocol;
	unsigned short checksum;
	unsigned int source_addr;
	unsigned int dest_addr;
};

// function for calculating the header checksum;
unsigned short compute_checksum(unsigned short *pBuffer, int length)
{
	unsigned int sum = 0;
	for (int i = 0; i < length; i++) {
		sum += ntohs(pBuffer[i]);
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return sum;
};

int stud_ip_recv(char *pBuffer, unsigned short length)
{
	ipv4 *packet = new(ipv4);
	packet = (ipv4*)pBuffer;

	// check the time to live
	unsigned int ttl = (ntohs(packet->ttl_protocol) >> 8) & 0xff;
	if (ttl <= 0) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
		return 1;
	}

	// check the version number (supposed to be 4)
	unsigned int version = (ntohs(packet->version_len_service) >> 12) & 0xf;
	if (version != 4) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
		return 1;
	}

	// check the head length (supposed to be bigger than 4)
	unsigned int head_len = (ntohs(packet->version_len_service) >> 8) & 0xf;
	if (head_len <= 4) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
		return 1;
	}

	// check the destination address (supposed to be host ip)
	if (ntohl(packet->dest_addr) != getIpv4Address()) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
		return 1;
	}

	// check the checksum
	unsigned short checksum_result = ~compute_checksum((unsigned short *)pBuffer, head_len * 2);
	if (checksum_result != 0) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
		return 1;
	}

	// valid data
	ip_SendtoUp(pBuffer, length);

	return 0;
}

int stud_ip_Upsend(char *pBuffer, unsigned short len, unsigned int srcAddr,
	unsigned int dstAddr, byte protocol, byte ttl)
{
	ipv4 *header = new(ipv4);
	header->version_len_service = htons(0x4500);
	header->total_length = htons(20 + len);
	header->tag = 0;
	header->flag_offset = 0;
	header->ttl_protocol = htons(((unsigned short)ttl << 8) + (unsigned short)protocol);
	header->checksum = 0;
	header->source_addr = htonl(srcAddr);
	header->dest_addr = htonl(dstAddr);

	// compute the checksum
	header->checksum = htons(~compute_checksum((unsigned short *)header, 10));

	// copy the data and send the packet
	memcpy((char *)header + 20, pBuffer, len);
	ip_SendtoLower((char *)header, len + 20);

	return 0;
}

