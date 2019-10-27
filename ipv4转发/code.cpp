/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <winsock.h>
using namespace std;

// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address();

// implemented by students
vector<stud_route_msg> route_list;

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

unsigned short compute_checksum(unsigned short *pBuffer, int length)
{
	unsigned int sum = 0;
	for (int i = 0; i < length; i++) {
		sum += ntohs(pBuffer[i]);
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return sum;
};

void stud_Route_Init()
{
	route_list.clear();
	return;
}

void stud_route_add(stud_route_msg *proute)
{
	route_list.push_back(*proute);
	return;
}


int stud_fwd_deal(char *pBuffer, int length)
{
	ipv4 *packet = new(ipv4);
	packet = (ipv4*)pBuffer;

	// check the time to live
	unsigned int ttl = (ntohs(packet->ttl_protocol) >> 8) & 0xff;
	if (ttl == 0) {
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
		return 1;
	}

	// should be received by host
	unsigned int dest_addr = ntohl(packet->dest_addr);
	if (dest_addr == getIpv4Address()) {
		fwd_LocalRcv(pBuffer, length);
		return 0;
	}
	
	stud_route_msg route_msg;
	for (int i = 0; i < route_list.size(); i++) {
		if (ntohl(route_list[i].dest) == dest_addr) {
			route_msg = route_list[i];
			break;
		}
		// cannot find it
		if (i == route_list.size() - 1) {
			fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
			return 1;
		}
	}

	// ttl-1, recompute the checksum
	unsigned int nexthop = ntohl(route_msg.nexthop);
	ttl--;
	unsigned int protocol = ntohs(packet->ttl_protocol) & 0xff;
	packet->ttl_protocol = htons(((unsigned short)ttl << 8) + (unsigned short)protocol);
	packet->checksum = 0;
	packet->checksum = htons(~compute_checksum((unsigned short *)packet, 10));
	fwd_SendtoLower((char*)packet, length, nexthop);

	return 0;
}

