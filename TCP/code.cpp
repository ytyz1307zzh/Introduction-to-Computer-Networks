/*
* THIS FILE IS FOR TCP TEST
*/

#include "sysInclude.h"
#include <iostream>
#include <winsock.h>
#include <vector>
using namespace std;

const unsigned short CLOSED = 0;
const unsigned short SYN_SENT = 1;
const unsigned short ESTABLISHED = 2;
const unsigned short FIN_WAIT_1 = 3;
const unsigned short FIN_WAIT_2 = 4;
const unsigned short TIME_WAIT = 5;
short gSrcPort = 2005;
short gDstPort = 2006;
int global_sockfd = 0;
/*
struct sockaddr_in {
	short   sin_family;
	u_short sin_port;
	struct  in_addr sin_addr;
	char    sin_zero[8];
};
*/

struct tcp_head
{
	unsigned short src_port;
	unsigned short dst_port;
	unsigned int seq;
	unsigned int ack;
	unsigned short length_flags;
	unsigned short window;
	unsigned short checksum;
	unsigned short urgent_ptr;
};

struct fake_head
{
	unsigned int src_addr;
	unsigned int dst_addr;
	unsigned short protocol;
	unsigned short length;
};

struct TCB
{
	unsigned int src_addr;
	unsigned int dst_addr;
	unsigned short src_port;
	unsigned short dst_port;
	unsigned short status;
	unsigned short src_seq;
	unsigned short dst_seq;
	int sockfd;
};

vector<TCB*> tcb_table;

unsigned short compute_checksum(unsigned short *pBuffer, int length, unsigned short *fake_head, int fake_length)
{
	unsigned int sum = 0;
	for (int i = 0; i < length; i++) {
		sum += ntohs(pBuffer[i]);
		sum = (sum >> 16) + (sum & 0xffff);
	}
	for (int i = 0; i < fake_length; i++) {
		sum += ntohs(fake_head[i]);
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return sum;
};


extern void tcp_DiscardPkt(char *pBuffer, int type);

extern void tcp_sendReport(int type);

extern void tcp_sendIpPkt(unsigned char *pData, UINT16 len, unsigned int  srcAddr, unsigned int dstAddr, UINT8	ttl);

extern int waitIpPacket(char *pBuffer, int timeout);

extern unsigned int getIpv4Address();

extern unsigned int getServerIpv4Address();

int stud_tcp_input(char *pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr)
{
	tcp_head* header = new(tcp_head);
	header = (tcp_head*)pBuffer;

	unsigned short dst_port = ntohs(header->src_port);
	unsigned short src_port = ntohs(header->dst_port);

	// find the correct tcb
	TCB* tcb = NULL;
	for (int i = 0; i < tcb_table.size(); i++) {
		if (tcb_table[i]->dst_addr == ntohl(srcAddr) && tcb_table[i]->src_addr == ntohl(dstAddr)
			&& tcb_table[i]->src_port == src_port && tcb_table[i]->dst_port == dst_port)
			tcb = tcb_table[i];
	}
	if (tcb == NULL) return -1;

	// check the sequence number
	unsigned int seq = ntohl(header->seq);
	if (seq != tcb->dst_seq) {
		tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
		return -1;
	}

	//change TCP status
	unsigned short flags = ntohs(header->length_flags) & 0x3f;
	if (tcb->status == SYN_SENT && flags == 0x12) {
		tcb->src_seq++;
		tcb->dst_seq++;
		stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->src_port, tcb->dst_port, tcb->src_addr, tcb->dst_addr);
		tcb->status = ESTABLISHED;
	}
	if (tcb->status == ESTABLISHED && flags == 0x10) {
		unsigned int ack = ntohl(header->ack);
		tcb->src_seq = ack;
		tcb->dst_seq = seq + 1;
	}
	if (tcb->status == FIN_WAIT_1 && flags == 0x10) {
		tcb->status = FIN_WAIT_2;
	}
	if (tcb->status == FIN_WAIT_2 && flags == 0x11) {
		tcb->src_seq++;
		tcb->dst_seq++;
		stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->src_port, tcb->dst_port, tcb->src_addr, tcb->dst_addr);
		tcb->status = TIME_WAIT;
	}
	return 0;
}

void stud_tcp_output(char *pData, unsigned short len, unsigned char flag, unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr)
{
	// for lab 1, there is no existing tcb_table
	TCB* tcb = NULL;
	if (tcb_table.size() == 0) {
		tcb = new TCB;
		tcb_table.push_back(tcb);
	}
	else {
		for (int i = 0; i < tcb_table.size(); i++) {
			if (tcb_table[i]->dst_addr == dstAddr && tcb_table[i]->src_addr == srcAddr
				&& tcb_table[i]->src_port == srcPort && tcb_table[i]->dst_port == dstPort)
				tcb = tcb_table[i];
		}
	}

	if (tcb == NULL) return;

	tcp_head* header = new(tcp_head);
	header->src_port = htons(srcPort);
	header->dst_port = htons(dstPort);
	header->window = htons(1);
	fake_head* fake_header = new(fake_head);
	fake_header->src_addr = htonl(srcAddr);
	fake_header->dst_addr = htonl(dstAddr);
	fake_header->protocol = htons(6);
	fake_header->length = htons(20+len);

	if (flag == PACKET_TYPE_SYN) {
		header->seq = htonl(1);
		header->ack = htonl(0);
		header->checksum = htons(0);
		header->length_flags = htons(0x5002);
		header->checksum = htons(~compute_checksum((unsigned short *)header, 10, (unsigned short *)fake_header, 6));
		tcp_sendIpPkt((unsigned char*)header, len + 20, srcAddr, dstAddr, 255);

		tcb->dst_addr = dstAddr;
		tcb->dst_port = dstPort;
		tcb->src_addr = srcAddr;
		tcb->src_port = srcPort;
		tcb->src_seq = 1;
		tcb->dst_seq = 1;
		tcb->status = SYN_SENT;
	}
	if (flag == PACKET_TYPE_ACK) {
		header->seq = htonl(tcb->src_seq);
		header->ack = htonl(tcb->dst_seq);
		header->checksum = htons(0);
		header->length_flags = htons(0x5010);
		header->checksum = htons(~compute_checksum((unsigned short *)header, 10, (unsigned short *)fake_header, 6));
		tcp_sendIpPkt((unsigned char*)header, len + 20, srcAddr, dstAddr, 255);
	}
	if (flag == PACKET_TYPE_FIN_ACK) {
		header->seq = htonl(tcb->src_seq);
		header->ack = htonl(tcb->dst_seq);
		header->checksum = htons(0);
		header->length_flags = htons(0x5011);
		header->checksum = htons(~compute_checksum((unsigned short *)header, 10, (unsigned short *)fake_header, 6));
		tcp_sendIpPkt((unsigned char*)header, len + 20, srcAddr, dstAddr, 255);

		if (tcb->status == ESTABLISHED)
			tcb->status = FIN_WAIT_1;
	}
	if (flag == PACKET_TYPE_DATA) {
		header->seq = htonl(tcb->src_seq);
		header->ack = htonl(tcb->dst_seq);
		header->checksum = htons(0);
		header->length_flags = htons(0x5000);
		memcpy((char *)header + 20, pData, len);
		header->checksum = htons(~compute_checksum((unsigned short *)header, 10 + len/2, (unsigned short *)fake_header, 6));
		tcp_sendIpPkt((unsigned char*)header, len + 20, srcAddr, dstAddr, 255);
	}
}

int stud_tcp_socket(int domain, int type, int protocol)
{
	global_sockfd++;
	TCB *tcb = new TCB;
	tcb->sockfd = global_sockfd;
	tcb_table.push_back(tcb);
	return global_sockfd;
}

int stud_tcp_connect(int sockfd, struct sockaddr_in *addr, int addrlen)
{
	unsigned int src_addr = getIpv4Address();
	unsigned int dst_addr = ntohl(addr->sin_addr.s_addr);
	unsigned short dst_port = ntohs(addr->sin_port);

	// find the correct socket
	TCB *tcb = NULL;
	for (int i = 0; i < tcb_table.size(); i++) {
		if (tcb_table[i]->sockfd == sockfd)
			tcb = tcb_table[i];
	}
	tcb->src_addr = src_addr;
	tcb->dst_addr = dst_addr;
	tcb->src_port = gSrcPort;
	tcb->dst_port = dst_port;

	stud_tcp_output(NULL, 0, PACKET_TYPE_SYN, gSrcPort, dst_port, src_addr, dst_addr);
	char* pBuffer = new char[100];
	int length = waitIpPacket(pBuffer, 255);
	stud_tcp_input(pBuffer, length, htonl(dst_addr), htonl(src_addr));
	return 0;
}

int stud_tcp_send(int sockfd, const unsigned char *pData, unsigned short datalen, int flags)
{
	// find the correct socket
	TCB *tcb = NULL;
	for (int i = 0; i < tcb_table.size(); i++) {
		if (tcb_table[i]->sockfd == sockfd)
			tcb = tcb_table[i];
	}

	stud_tcp_output((char*)pData, datalen, PACKET_TYPE_DATA, tcb->src_port, tcb->dst_port, tcb->src_addr, tcb->dst_addr);
	char* pBuffer = new char[100];
	int length = waitIpPacket(pBuffer, 255);
	if (length == -1) return -1;
	stud_tcp_input(pBuffer, length, htonl(tcb->dst_addr), htonl(tcb->src_addr));

	return 0;
}

int stud_tcp_recv(int sockfd, unsigned char *pData, unsigned short datalen, int flags)
{
	TCB *tcb = NULL;
	for (int i = 0; i < tcb_table.size(); i++) {
		if (tcb_table[i]->sockfd == sockfd)
			tcb = tcb_table[i];
	}
	if (tcb->status != ESTABLISHED)
		return -1;

	char* pBuffer = new char[100];
	int length = waitIpPacket(pBuffer, 255);
	if (length == -1) return -1;

	tcp_head *header = new tcp_head;
	memcpy((char*)header, pBuffer, 20);
	memcpy(pData, (unsigned char *)pBuffer + 20, length - 20);
	tcb->src_seq = ntohl(header->ack);
	tcb->dst_seq = ntohl(header->seq) + length - 20;
	stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->src_port, tcb->dst_port, tcb->src_addr, tcb->dst_addr);

	return 0;
}

int stud_tcp_close(int sockfd)
{
	TCB *tcb = NULL;
	for (int i = 0; i < tcb_table.size(); i++) {
		if (tcb_table[i]->sockfd == sockfd)
			tcb = tcb_table[i];
	}

	// send FIN
	stud_tcp_output(NULL, 0, PACKET_TYPE_FIN_ACK, tcb->src_port, tcb->dst_port, tcb->src_addr, tcb->dst_addr);
	tcb->src_seq++;
	tcb->dst_seq++;

	// wait for ACK
	char* pBuffer = new char[100];
	int length = waitIpPacket(pBuffer, 255);
	if (length == -1) return -1;
	stud_tcp_input(pBuffer, length, htonl(tcb->dst_addr), htonl(tcb->src_addr));

	// wait for FIN
	length = waitIpPacket(pBuffer, 255);
	if (length == -1) return -1;

	// send ACK
	stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->src_port, tcb->dst_port, tcb->src_addr, tcb->dst_addr);
	return 0;
}
