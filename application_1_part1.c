#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "stm32f4xx_dac.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

const bool VERBOSE = true;

const int DAC_WR_ADDR = DAC_Trigger_T5_TRGO + DAC_BASE;
#define DAC_wr_pointer ((volatile unsigned char *)0x4000741C)

const int melody_notes[] = {
  0, 2, 4, 0, 0, 2, 4, 0, 
  4, 5, 7, 4, 5, 7, 7, 9, 
  7, 5, 4, 0, 7, 9, 7, 5, 
  4, 0, 0, -5, 0, 0, -5, 0
};
const int per_array[] = {
  2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351,
  1275, 1203, 1136, 1072, 1012,  955,  901,  851,
   803,  758,  715,  675,  637,  601,  568,  536,
   506
};
unsigned int freq_idx_2_arr(unsigned int in) {
  return in + 10;
}

typedef struct {
    Object super;
    char str_buff[128];
    int str_index;
    int int_buff[3];
    int int_index;
    int int_count;
} App;

typedef struct {
  Object super;
  int volume;
  int muted;
} DAC_obj;

#define initApp() { initObject(), {}, 0, {}, 0, 0}
#define initDAC() { initObject(), 5}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
DAC_obj dac_obj = initDAC();
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

void DAC_set_vol(DAC_obj *self, int vol){
  self->volume = vol;
}

void DAC_mute(DAC_obj *self, int dummy){
  if(self->muted == 1)
    self->muted = 0;
  else
    self->muted = 1;
}

void DAC_wr(DAC_obj *self, int play){ // 0 -> silent, 1 -> sound (setted volume)
  int next = play;
  if(play == 0 || self->muted == 1){
    *DAC_wr_pointer = 0;
	  next = 1;
  }else{
    *DAC_wr_pointer = self->volume;
    next = 0;
  }
  
  AFTER(USEC(500), &dac_obj, DAC_wr, next);
}

void reader(App *self, int c) {
    int bufferValue;
    int sum;
    int median;
    if(VERBOSE){
      if (c == '\n')
        return;
      print("Rcv: '%c'\n", c);
    }
    switch (c) {
      case 'm'://mute
        DAC_mute(&dac_obj, 0);
        break;
      case 'v'://press v to confirm volume change
        self->str_buff[self->str_index] = '\0';
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if (bufferValue<1){
          bufferValue=1;
          print("Minimum volume is 1",0);
        }else if (bufferValue>10){
          bufferValue=10;
          print("Maximum volume is 10",0);
        }
        DAC_set_vol(&dac_obj,bufferValue);
        break;
    case 'k':
      self->str_buff[self->str_index] = '\0';
      self->str_index = 0;
      bufferValue = atoi(self->str_buff);

      print("Key: %d\n", bufferValue);

      for(int i = 0; i < 32; i++){
        print("%d", per_array[freq_idx_2_arr(melody_notes[i] + bufferValue)]);
        if(i != 31)
          print(", ",0);
      }
      print("\n", 0);
      
      break;
    case 'e':
      self->str_buff[self->str_index] = '\0';
      self->str_index = 0;
      bufferValue = atoi(self->str_buff);

      self->int_buff[self->int_index++%3] = bufferValue;
      self->int_count = (self->int_count >= 3) ? 3 : self->int_count+1;

      sum = 0;
      for (unsigned short int i = 0; i < self->int_count; i++){
        sum += self->int_buff[i];
      }
      
      if(self->int_count == 1){
        median = sum;
      }else if(self->int_count == 2){
        median = sum/2;
      }else{
        if ((self->int_buff[0] >= self->int_buff[1] && self->int_buff[0] <= self->int_buff[2]) || (self->int_buff[0] <= self->int_buff[1] && self->int_buff[0] >= self->int_buff[2])) {
            median = self->int_buff[0]; // self->int_buff[0] is the middle value
        } else if ((self->int_buff[1] >= self->int_buff[0] && self->int_buff[1] <= self->int_buff[2]) || (self->int_buff[1] <= self->int_buff[0] && self->int_buff[1] >= self->int_buff[2])) {
            median = self->int_buff[1]; // self->int_buff[1] is the middle value
        } else {
            median = self->int_buff[2]; // self->int_buff[2] is the middle value
        }
      }
      print("Entered integer %d: ", bufferValue);
	    print("sum = %d, ", sum);
	    print("median = %d\n", median);
      break;
    case 'f':
      self->str_index = 0;
      self->int_index = 0;
      self->int_count = 0;
      print("The 3-history has been erased.\n",0);
      break;
    default:
      self->str_buff[self->str_index++] = c;
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

    AFTER(USEC(500), &dac_obj, DAC_wr, 1);

}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
