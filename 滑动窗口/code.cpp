#include "sysinclude.h"
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <cstring>
#include <winsock.h>
using namespace std;

extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4

typedef enum {DATA, ACK, NAK} frame_kind;
typedef struct frame_head
{
	unsigned int kind;
	unsigned int seq;
	unsigned int ack;
	char data[100];
};
typedef struct frame
{
	frame_head head;
	unsigned int size;
};

vector <char*> buffer; // sending queue buffer
int top = 0, bottom = 0; // window boundaries

/*
* 停等协议测试函数
*/
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType)
{
	frame_head *fr = new(frame_head);
	memcpy(fr, pBuffer, bufferSize);

	if (messageType == MSG_TYPE_SEND) {
		if (ntohl(fr -> seq) == 1) {
			buffer.clear();
			top = 0; bottom = 0;
		}
		int width = top - bottom;
		buffer.push_back((char*)fr);

		if (width >= WINDOW_SIZE_STOP_WAIT)
			return 0;
		top++; //open a window
		SendFRAMEPacket((unsigned char *)pBuffer, bufferSize);
	}
	
	else if (messageType == MSG_TYPE_RECEIVE) {
		int ack_cnt = ntohl(fr -> ack) - bottom; // how many frames have been received
		bottom += ack_cnt;

		if (buffer.size() >= top + 1) { // there are waiting frames to send
			for (int j = top; j < top + ack_cnt; j++) {
				frame_head *wait_fr = (frame_head *) buffer[j];
				SendFRAMEPacket((unsigned char *)buffer[j], strlen((wait_fr -> data)) + 13);
			}
			top += ack_cnt;
		}
	}

	else if (messageType == MSG_TYPE_TIMEOUT) {		
		for (int j = bottom; j < top; j++) {
			frame_head *wait_fr = (frame_head *) buffer[j];
			SendFRAMEPacket((unsigned char *)buffer[j], strlen((wait_fr -> data)) + 13);
		}
			
	}
		
	return 0;


}

/*
* 回退n帧测试函数
*/
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType)
{
	frame_head *fr = new(frame_head);
	memcpy(fr, pBuffer, bufferSize);

	if (messageType == MSG_TYPE_SEND) {
		if (ntohl(fr -> seq) == 1) {
			buffer.clear();
			top = 0; bottom = 0;
		}
		int width = top - bottom;
		buffer.push_back((char*)fr);

		if (width >= WINDOW_SIZE_BACK_N_FRAME)
			return 0;
		top++; //open a window
		SendFRAMEPacket((unsigned char *)pBuffer, bufferSize);
	}
	
	else if (messageType == MSG_TYPE_RECEIVE) {
		int ack_cnt = ntohl(fr -> ack) - bottom; // how many frames have been received
		bottom += ack_cnt;

		if (buffer.size() >= top + 1) { // there are waiting frames to send
			for (int j = top; j < top + ack_cnt; j++) {
				frame_head *wait_fr = (frame_head *) buffer[j];
				SendFRAMEPacket((unsigned char *)buffer[j], strlen((wait_fr -> data)) + 13);
			}
			top += ack_cnt;
		}
	}

	else if (messageType == MSG_TYPE_TIMEOUT) {		
		for (int j = bottom; j < top; j++) {
			frame_head *wait_fr = (frame_head *) buffer[j];
			SendFRAMEPacket((unsigned char *)buffer[j], strlen((wait_fr -> data)) + 13);
		}
			
	}
		
	return 0;

}

/*
* 选择性重传测试函数
*/
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType)
{
	frame_head *fr = new(frame_head);
	memcpy(fr, pBuffer, bufferSize);

	if (messageType == MSG_TYPE_SEND) {
		if (ntohl(fr -> seq) == 1) {
			buffer.clear();
			top = 0; bottom = 0;
		}
		int width = top - bottom;
		buffer.push_back((char*)fr);

		if (width >= WINDOW_SIZE_BACK_N_FRAME)
			return 0;
		top++; //open a window
		SendFRAMEPacket((unsigned char *)pBuffer, bufferSize);
	}
	
	else if (messageType == MSG_TYPE_RECEIVE && ntohl(fr -> kind) == ACK) {
		int ack_cnt = ntohl(fr -> ack) - bottom; // how many frames have been received
		bottom += ack_cnt;

		if (buffer.size() >= top + 1) { // there are waiting frames to send
			for (int j = top; j < top + ack_cnt; j++) {
				frame_head *wait_fr = (frame_head *) buffer[j];
				SendFRAMEPacket((unsigned char *)buffer[j], strlen((wait_fr -> data)) + 13);
			}
			top += ack_cnt;
		}
	}

	else if (messageType == MSG_TYPE_RECEIVE && ntohl(fr -> kind) == NAK) {
		int nak_num = ntohl(fr -> ack) - 1;		
		frame_head *wait_fr = (frame_head *) buffer[nak_num];
		SendFRAMEPacket((unsigned char *)buffer[nak_num], strlen((wait_fr -> data)) + 13);
			
	}
		
	return 0;
}
