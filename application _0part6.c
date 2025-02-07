#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>

const int freq_array[32]={0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
const int per_array[25]={2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};
typedef struct {
    Object super;
	char buffer[128];
	int index;
} App;

#define initApp() { initObject(), {}, 0}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void print(char *format, int arg) {
  char buf[128];
  snprintf(buf, 128, format,arg);
  SCI_WRITE(&sci0, buf);
}

void period_writer (int keyValue){
	print("Key: '%d'\n", keyValue);
	for (int i=0;i<=31;i++){
		print("%d ",per_array[freq_array[i]+keyValue+10]);
	}
	print("\n",0);
}

void reader(App *self, int c) {
    int bufferValue;
	if (c == '\n'){
		return;
	}
	switch (c){
		case 'k':
			self->buffer[self->index] = '\0';
			self->index = 0;
			bufferValue = atoi(self->buffer);
			period_writer(bufferValue);
			break;
		default:
			self->buffer[self->index++] = c;
			break;
	}
}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
