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
	Time tic;
	Time exec_time;
	Time max;
	Time totaltime;
	Time average;
	int count;
} Dac_obj;

typedef struct {
    Object super;
	int background_loop_range;
	int l_deadline;
	Time tic;
	Time toc;
	Time exec_time;
	Time max;
	Time totaltime;
	Time average;
	int count;
} Load_obj;

#define initApp() { initObject(), {}, 0}
#define initDac_obj() { initObject(), 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define initLoad_obj() { initObject(), 1000, 0, 0, 0, 0, 0, 0, 0, 0}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN_PORT0, &app, receiver);
Dac_obj obj_dac = initDac_obj();
Load_obj distort = initLoad_obj();

void receiver(App *self, int unused) {
    CANMsg msg;
void print(char *format, int arg) {
  SCI_WRITE(&sci0, buf);
}

//void period_writer (int keyValue){
//	print("Key: '%d'\n", keyValue);
//	for (int i=0;i<=31;i++){
//		print("%d ",per_array[freq_array[i]+keyValue+10]);
//	}
//	print("\n",0);
//}

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
	print("\n",0);
}

void dac_wr(Dac_obj *self, int unused){
	self->tic = USEC_OF(CURRENT_OFFSET());	
	
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
		AFTER(USEC(500), &obj_dac, dac_wr, 0);//Change USEC parameter to change the period task; this is half of the tone's wave period
	else
		SEND(USEC(500), USEC(100), &obj_dac, dac_wr, 0);//Change first USEC parameter to change the period task; this is half of the tone's wave period
		
	self->exec_time = USEC_OF(CURRENT_OFFSET()) - self->tic;
	if (self->exec_time>self->max)
		self->max = self->exec_time;
	self->totaltime = self->totaltime + self->exec_time;
	self->count++;
	if(self->count==500){
		self->average = self->totaltime/500;
		print("Average WCET is %d microseconds.\n",self->average);
		print("Maximum WCET is %d microseconds.\n",self->max);
	}
}	

void load(Load_obj *self, int unused){
	self->tic = USEC_OF(CURRENT_OFFSET());
	
	for(int i=0; i<self->background_loop_range; i++){
	}
	if (self->l_deadline==0)
		AFTER(USEC(1300), &distort, load, 0);
	else
		SEND(USEC(1300), USEC(1300), &distort, load, 0);
		
	self->toc=USEC_OF(CURRENT_OFFSET());
	self->exec_time = self->toc - self->tic;
	if (self->exec_time>self->max)
		self->max = self->exec_time;
	self->totaltime = self->totaltime + self->exec_time;
	self->count++;
	if(self->count==500){
		self->average = self->totaltime/500;
		print("Average WCET is %d microseconds.\n",self->average);
		print("Maximum WCET is %d microseconds.\n",self->max);
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
			
void startApp(App *self, int arg) {
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
	
//	AFTER(USEC(500), &obj_dac, dac_wr, 0);
	AFTER(USEC(1300), &distort, load, 0);
}

int main() {
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
