//User guide:
//Press 'v' to input the desired volume
//Press 'm' to mute and again to unmute
//Press 'u' and 'd' to increase and decrease distortion, respectively
//Press 'q' to enable and disable deadlines

#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "stm32f4xx_dac.h"
#include <stdlib.h>
#include <stdio.h>


const int freq_array[32]={0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
const int per_array[25]={2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

const int DAC_WR_ADDR = DAC_Trigger_T5_TRGO + DAC_BASE;
#define DAC_wr_pointer ((volatile unsigned char *)0x4000741C)

typedef struct {
    Object super;
	char buffer[128];
	int index;
} App;

typedef struct {
    Object super;
	
	int vol;
	int muted;
	int play;
	int d_deadline;
	
	int d_tic;
	int d_totaltime;
	int d_average;
	int d_maxtime;
	int d_exec_time[500];
	
	int d_count;
	int d_run;
} Dac_obj;

typedef struct {
    Object super;
	
	int background_loop_range;
	int l_deadline;
	
	int l_tic;
	int l_totaltime;
	int l_average;
	int l_maxtime;
	int l_exec_time[500];
	
	int l_count;
	int l_run;
} Load_obj;

#define initApp() { initObject(), {}, 0}
#define initDac_obj() { initObject(), 1, 0, 0, 0, 0, 0, 0, 0, {}, 0, 0}
#define initLoad_obj() { initObject(), 13500, 0, 0, 0 ,0 , 0, {}, 0, 0}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);
Dac_obj obj_dac = initDac_obj();
Load_obj distort = initLoad_obj();

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

void dac_vol(Dac_obj *self, int v){
	self->vol = v;
}

void dac_mute(Dac_obj *self, int unused){
	if (self->muted==0)
		self->muted=1;
	else
		self->muted=0;
}

void dac_deadline(Dac_obj *self, int unused){
	if (self->d_deadline==0)
		self->d_deadline=1;
	else
		self->d_deadline=0;
}

void load_deadline(Load_obj *self, int unused){
	if (self->l_deadline==0){
		self->l_deadline=1;
		print("Deadlines enabled.\n",0);
	}else{
		self->l_deadline=0;
		print("Deadlines disabled.\n",0);
	}
}

void dac_wr(Dac_obj *self, int unused){
	
	if (self->play==0){
		*DAC_wr_pointer = 0;
		self->play=1;
	}else if (self->muted==1){
		*DAC_wr_pointer = 0;
		self->play=0;
	}else{
		*DAC_wr_pointer = self->vol;
		self->play=0;
	}

	if (self->d_deadline==0)
		AFTER(SEC(1), &obj_dac, dac_wr, 0);//Change USEC parameter to change the period task; this is half of the tone's wave period
	else
		SEND(USEC(500), USEC(100), &obj_dac, dac_wr, 0);//Change first USEC parameter to change the period task; this is half of the tone's wave period
	
}	

void load(Load_obj *self, int unused){	
	for(int i=0; i<self->background_loop_range; i++){
	}
	if (self->l_deadline==0)
		AFTER(USEC(1300), &distort, load, 0);
	else
		SEND(USEC(1300), USEC(1300), &distort, load, 0);
	
	self->l_count++;
	if (self->l_count==999){
		self->l_count = 0;
		self->l_exec_time[self->l_run] = USEC_OF(CURRENT_OFFSET())-self->l_tic;
		self->l_tic = CURRENT_OFFSET();
		self->l_totaltime = self->l_totaltime + self->l_exec_time[self->l_run];
		self->l_exec_time[self->l_run] = self->l_exec_time[self->l_run]/1000;
		if(self->l_exec_time[self->l_run]>self->l_maxtime)
			self->l_maxtime=self->l_exec_time[self->l_run];
		self->l_run++;
		print("Run: %d.\n",self->l_run);
		if(self->l_run==500){
			self->l_average = (self->l_totaltime/1000)/500;
			print("Average WCET is %d.\n",self->l_average);
			print("Maximum WCET is %d.\n",self->l_maxtime);
		}
	}

}

void load_control(Load_obj *self, int k){
	if (k==0){
		self->background_loop_range = self->background_loop_range-500;
		if (self->background_loop_range<0){
			self->background_loop_range=0;
			print("Minimum background loop range is 0.\n",0);
		}
	}else{
		self->background_loop_range = self->background_loop_range+500;
		if (self->background_loop_range>10000){
			self->background_loop_range=10000;
			print("Maximum background loop range is 10000.\n",0);
		}
	}
	print("New background loop range is %d.\n",self->background_loop_range);
}

//void period_writer (int keyValue){
//	print("Key: '%d'\n", keyValue);
//	for (int i=0;i<=31;i++){
//		print("%d ",per_array[freq_array[i]+keyValue+10]);
//	}
//	print("\n",0);
//}

void reader(App *self, int c) {
    int bufferValue;
	if (c == '\n'){
		return;
	}
	switch (c){
		case 'm'://mute
			dac_mute(&obj_dac, 0);
			break;
		case 'v'://press v to confirm volume change
			self->buffer[self->index] = '\0';
			self->index = 0;
			bufferValue = atoi(self->buffer);
			if (bufferValue<1){
				bufferValue=1;
				print("Minimum volume is 1.\n",0);
			}else if (bufferValue>10){
				bufferValue=10;
				print("Maximum volume is 10.\n",0);
			}
			dac_vol(&obj_dac,bufferValue);
			print("New volume is %d.\n",bufferValue);
			break;
		case 'u'://increase distortion
			load_control(&distort, 1);
			break;
		case 'd'://decrease distortion
			load_control(&distort, 0);
			break;
		case 'q'://enable and disable deadlines
			dac_deadline(&obj_dac, 0);
			load_deadline(&distort, 0);
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
	
	AFTER(USEC(500), &obj_dac, dac_wr, 0);
//	AFTER(USEC(1300), &distort, load, 0);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}