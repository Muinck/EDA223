//User guide:
//Press 'v' to input the desired volume
//Press 'm' to mute and again to unmute
//Press 'u' and 'd' to increase and decrease distortion, respectively
//Press 'q' to enable and disable deadlines
<<<<<<< Updated upstream
//Press 'h' to change the frequency
//Press 'p' to print the user guide
=======
>>>>>>> Stashed changes

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

<<<<<<< Updated upstream
=======

const int freq_array[32]={0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};
const int per_array[25]={2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

const int DAC_WR_ADDR = DAC_Trigger_T5_TRGO + DAC_BASE;
#define DAC_wr_pointer ((volatile unsigned char *)0x4000741C)

>>>>>>> Stashed changes
typedef struct {
    Object super;
    char str_buff[128];
    int str_index;
    int int_buff[3];
    int int_index;
    int int_count;
} App;

typedef struct {
<<<<<<< Updated upstream
  Object super;
  int volume;
  int muted;
  int period;
	int d_deadline;
	
	int d_tic;
	int d_totaltime;
	int d_average;
	int d_maxtime;
	int d_exec_time[500];
	
	int d_count;
	int d_run;
} DAC_obj;

typedef struct {
  Object super;
  int loop_range;
	int l_deadline;
  
	int l_tic;
	int l_totaltime;
	int l_average;
	int l_maxtime;
	int l_exec_time[500];
	
	int l_count;
	int l_run;
} loop_load;

#define initApp() { initObject(), {}, 0, {}, 0, 0}
#define initDAC() { initObject(), 5, 0, 500, 0, 0, 0, 0, 0, {}, 0, 0}
#define initload() { initObject(), 1000, 0, 0, 0 ,0 , 0, {}, 0, 0}
=======
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
>>>>>>> Stashed changes

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
DAC_obj obj_dac = initDAC();
loop_load load = initload();
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

<<<<<<< Updated upstream
int add_loop_range(loop_load *self, int dummy){
  self->loop_range = self->loop_range + 500;
  return self->loop_range;
}
int sub_loop_range(loop_load *self, int dummy){
	if(self->loop_range >= 500)
    self->loop_range = self->loop_range - 500;
  return self->loop_range;
}


void empty_loop(loop_load *self, int unused){	
	for(int i=0; i<self->loop_range; i++){
	}
	if (self->l_deadline==0)
		AFTER(USEC(1300), self, empty_loop, 0);
	else
		SEND(USEC(1300), USEC(1300), self, empty_loop, 0);
	
//	self->l_count++;
//	if (self->l_count==999){
//		self->l_count = 0;
//		self->l_exec_time[self->l_run] = USEC_OF(CURRENT_OFFSET())-self->l_tic;
//		self->l_tic = CURRENT_OFFSET();
//		self->l_totaltime = self->l_totaltime + self->l_exec_time[self->l_run];
//		self->l_exec_time[self->l_run] = self->l_exec_time[self->l_run]/1000;
//		if(self->l_exec_time[self->l_run]>self->l_maxtime)
//			self->l_maxtime=self->l_exec_time[self->l_run];
//		self->l_run++;
//		print("Run: %d.\n",self->l_run);
//		if(self->l_run==500){
//			self->l_average = (self->l_totaltime/1000)/500;
//			print("Average WCET is %d.\n",self->l_average);
//			print("Maximum WCET is %d.\n",self->l_maxtime);
//		}
//	}
}

void dac_deadline(DAC_obj *self, int unused){
	if (self->d_deadline==0)
		self->d_deadline=1;
	else
		self->d_deadline=0;
}

void load_deadline(loop_load *self, int unused){
	if (self->l_deadline==0){
		self->l_deadline=1;
		print("Deadlines enabled.\n",0);
	}else{
		self->l_deadline=0;
		print("Deadlines disabled.\n",0);
	}
}

void DAC_set_vol(DAC_obj *self, int vol){
  self->volume = vol;
}

void DAC_mute(DAC_obj *self, int dummy){
  if(self->muted == 1){
    self->muted = 0;
    print("DAC muted", 0);
  }else{
    print("DAC unmuted", 0);
    self->muted = 1;
  }
}

void DAC_set_freq(DAC_obj *self, int freq){
  int period = 1000000/(2*freq);
  self->period = period;
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
  	if (self->d_deadline==0)
		AFTER(USEC(self->period), self, DAC_wr, next);
	else
		SEND(USEC(self->period), USEC(100), self, DAC_wr, next);
=======
void dac_vol(Dac_obj *self, int v){
	self->vol = v;
>>>>>>> Stashed changes
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
	//print("Current time is %t.\n",self->tic);
	
	for(int i=0; i<self->background_loop_range; i++){
	}
	if (self->l_deadline==0)
		AFTER(USEC(1300), &distort, load, 0);
	else
		SEND(USEC(1300), USEC(1300), &distort, load, 0);
	self->toc=USEC_OF(CURRENT_OFFSET());
	self->exec_time = self->toc - self->tic;
	//print("Current time is %t.\n",self->exec_time);
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

//void period_writer (int keyValue){
//	print("Key: '%d'\n", keyValue);
//	for (int i=0;i<=31;i++){
//		print("%d ",per_array[freq_array[i]+keyValue+10]);
//	}
//	print("\n",0);
//}

void reader(App *self, int c) {
<<<<<<< Updated upstream
  int bufferValue;
  int sum;
  int median;
  if(VERBOSE){
    if (c == '\n')
      return;
    print("Rcv: '%c'\n", c);
  }
  switch (c) {
	case 'p':
      print("p: shows this message\n",0);
      print("m: mutes the DAC\n",0);
      print("h: sets the frequency of the DAC in hz\n",0);
      print("u: increases loop range by 500\n",0);
      print("d: decreases loop range by 500\n",0);
      print("<int>v: sets the volume to <int>\n",0);
      print("<int>k: changes the playing key to <int>\n",0);
      print("q: enables/disables deadlines\n",0);
      print("<int>e: adds number <int> to the list and prints sum and median of the list\n",0);
      print("f: history list erased\n",0);
      break;
    case 'm'://mute
      DAC_mute(&obj_dac, 0);
      break;
    case 'h'://press h to set the freq in heartz
      self->str_buff[self->str_index] = '\0';
      self->str_index = 0;
      bufferValue = atoi(self->str_buff);
      if (bufferValue<0){
        bufferValue=0;
        print("Minimum freq is 0 heartzs",0);
      }
      DAC_set_freq(&obj_dac, bufferValue);
      print("Setting DAC frequency to %d\n", bufferValue);
      break;
    case 'u'://press u to increase the loop range
      bufferValue = add_loop_range(&load, 0);
      print("Increasing loop range to %d\n", bufferValue);
      break;
    case 'd'://press d to decreaset the loop range
      bufferValue = sub_loop_range(&load, 0);
      print("Decreasing loop range to %d\n", bufferValue);
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
      print("Volume set to: %d\n", bufferValue);
      DAC_set_vol(&obj_dac,bufferValue);
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
  case 'q'://enable and disable deadlines
    dac_deadline(&obj_dac, 0);
    load_deadline(&load, 0);
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
=======
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
>>>>>>> Stashed changes
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

  AFTER(USEC(1300), &load, empty_loop, 0);
  AFTER(USEC(500), &obj_dac, DAC_wr, 1);

<<<<<<< Updated upstream
=======
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
	
//	AFTER(USEC(500), &obj_dac, dac_wr, 0);
	AFTER(USEC(1300), &distort, load, 0);
>>>>>>> Stashed changes
}

int main() {
	INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
<<<<<<< Updated upstream
	TINYTIMBER(&app, startApp, 0);
  return 0;
=======
    TINYTIMBER(&app, startApp, 0);
    return 0;
>>>>>>> Stashed changes
}