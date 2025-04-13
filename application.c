//Press 'p' to print the user guide

#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "stm32f4xx_dac.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

bool VERBOSE = false;
const int  c_nodeId = 15;

const int DAC_WR_ADDR = DAC_Trigger_T5_TRGO + DAC_BASE;
#define DAC_wr_pointer ((volatile unsigned char *)0x4000741C)

const int can_node_id = 1;

typedef struct {
  Object super;
  int msg_cnt;
  int abort_burst;

  //rcv
  int cooldown;
  int can_buff[10];
  int can_head;  // index of the next item to remove (read)
  int can_tail;  // index where the next item will be inserted (write)
  int can_size;  // current number of elements in the buffer (optional but useful)
} App;

#define initApp() { initObject(), 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);

void print(char *format, int arg) {
  char buf[128];
  snprintf(buf, 128, format,arg);
  SCI_WRITE(&sci0, buf);
}

bool can_buffer_push(int buff[10], int* head, int* tail, int* size, int value) {
  if (*size == 10) {
      return false; // Buffer full
  }

  buff[*tail] = value;
  *tail = (*tail + 1) % 10;
  (*size)++;
  return true;
}

int can_buffer_pop(int buff[10], int* head, int* tail, int* size) {
  if (*size == 0) {
      return -1; // Buffer empty
  }

  int value = buff[*head];
  *head = (*head + 1) % 10;
  (*size)--;
  return value;
}

void get_msg(CANMsg in){
  print("Aplication got CAN with id %d\n", in.msgId);
}

void rst_cooldown(App *self, int dummy){
  if(self->can_size == 0){
    self->cooldown = 0;
  }else{
    CANMsg msg;
    msg.length = 0;
    msg.nodeId = c_nodeId;
    msg.msgId = can_buffer_pop(self->can_buff, &self->can_head, &self->can_tail, &self->can_size);
    get_msg(msg); //send next msg to app

    AFTER(MSEC(1000), self, rst_cooldown, 0);
  }
}

// receives messages from CAN
void receiver(App *self, int unused) {
	CANMsg msg;
	CAN_RECEIVE(&can0, &msg);
  
  if(self->cooldown == 1){
    if(!can_buffer_push(self->can_buff, &self->can_head, &self->can_tail, &self->can_size, msg.msgId)){
      print("Buffer full, msg %d discarded\n", msg.msgId);
    }
  }else{
    self->cooldown = 1;
    get_msg(msg); //send msg to app
    AFTER(MSEC(1000), self, rst_cooldown, 0);
  }
}

void can_write(App *self, int id){
	CANMsg can_msg;
  can_msg.msgId = id & 0x7F;
  can_msg.nodeId = c_nodeId;
  can_msg.length = 0;
  
  CAN_SEND(&can0, &can_msg);
}

void send_msg(App *self, int dummy){
  can_write(self, self->msg_cnt);
  if(VERBOSE){
    print("Sending CAN message with id %d\n", self->msg_cnt);
  }
  self->msg_cnt = (self->msg_cnt+1)%128;
}

void send_burst(App *self, int dummy){
  if(self->abort_burst == 0){
    send_msg(self, dummy);
    AFTER(MSEC(500), self, send_burst, dummy);
  }
}

// Keyboard 
void reader(App *self, int c) {
	
  switch (c) {
    case 'h':
      print("h: shows this message\n",0);
      print("O: sends a single can message\n",0);
      print("B: burst, sends a can message every 500 ms\n",0);
      print("X: cancels the burst\n",0);
      print("V: verbose mode prints when sending CAN messages\n",0);
      break;
    case 'O': //send single message
      send_msg(self, 0);
      break;
    case 'B': //send burst
      print("Starting to send burst\n", 0);
      self->abort_burst = 0;
      ASYNC(self, send_burst, 0);
      break;
    case 'V': //send burst
      VERBOSE = !VERBOSE;
      if(VERBOSE){
        print("Now in verbose mode\n", 0);
      }else{
        print("Now in quiet mode\n", 0);

      }
      break;
    case 'X': //cancell burst
      print("Cancelling burst\n", 0);
      self->abort_burst = 1;
      break;
    default:
      break;
  }
}

void startApp(App *self, int arg) {
  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  SCI_WRITE(&sci0, "Hello, hello...\n");

}

int main() {
	INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
	TINYTIMBER(&app, startApp, 0);
  return 0;
}