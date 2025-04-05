//Press 'p' to print the user guide

#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "stm32f4xx_dac.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

const bool VERBOSE = true;

const int DAC_WR_ADDR = DAC_Trigger_T5_TRGO + DAC_BASE;
#define DAC_wr_pointer ((volatile unsigned char *)0x4000741C)

const int can_node_id = 1;

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

const float note_dur[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  2, 1, 1, 2, 0.5, 0.5, 0.5, 0.5, 1, 1,
  0.5, 0.5, 0.5, 0.5, 1, 1, 1, 1, 2, 1,
  1, 2
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
  int can_mode;       // app will receive str for can msg
  int conductor_mode; // when conductor mode == 1 this board controls the other ones
                      // when mussician (not conductor == 0) plays what it receives
} App;

typedef struct {
  Object super;
  int volume;
  int muted;
  int gap;
  int period;
  int d_deadline;
  int play;
} DAC_obj;

typedef struct {
  Object super;
  int tempo; // period in ms
  int gap_siz; // ms //cte
  int key;
  int kill;
  int mel_idx;
} Mel_obj;

typedef struct {
  Object super;
  int loop_range;
	int l_deadline;
} loop_load;

#define initApp() { initObject(), {}, 0, {}, 0, 0, 0, 1}
#define initDAC() { initObject(), 3, 0, 0, 500, 0, 0}
#define initload() { initObject(), 1000, 0}
#define initMel() { initObject(), 60000000/120, 50, 0, 0, 0}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
DAC_obj obj_dac = initDAC();
Mel_obj mel_obj = initMel();
loop_load load = initload();
Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);

void print(char *format, int arg) {
  char buf[128];
  snprintf(buf, 128, format,arg);
  SCI_WRITE(&sci0, buf);
}

int bpm2tempo(int bpm){
  return 60000000/bpm; // us
}

void measure_empty_loop(loop_load *self, int unused){	
	for(int i=0; i<self->loop_range; i++){
	}
}

void measure_tonegen(DAC_obj *self){	
	if(self->play == 0 || self->muted == 1){
		*DAC_wr_pointer = 0;
		self->play = 1;
	}else{
		*DAC_wr_pointer = self->volume;
		self->play = 0;
	}
}

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
}

void dac_deadline(DAC_obj *self, int unused){
	if (self->d_deadline==0)
		self->d_deadline=1;
	else
		self->d_deadline=0;
}

void mel_set_tempo(Mel_obj *self, int tempo){
  self->tempo = bpm2tempo(tempo);
}

void mel_set_key(Mel_obj *self, int key){
  self->key = key;
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

void DAC_gap(DAC_obj *self, int gap){
  self->gap = gap;
}

void Mel_kill(Mel_obj *self, int kill){
  self->kill = kill;
}

void DAC_set_freq(DAC_obj *self, int freq){
  int period = 1000000/(2*freq);
  self->period = period;
}

void DAC_set_period(DAC_obj *self, int period){
  self->period = period;
}

void DAC_wr(DAC_obj *self, int play){ // 0 -> silent, 1 -> sound (setted volume)
  int next = play;
  if(play == 0 || self->muted == 1 || self->gap == 1){
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
}

void play_song_funct(Mel_obj *self, int in){
  // 0 -> note, 1 -> gap

  if(self->kill == 1){
    // mute
    SYNC(&obj_dac, DAC_gap, 1);
    self->mel_idx = 0;
    return;
  }
  if(in == 0){
    // set new tone
    SYNC(&obj_dac, DAC_set_period, per_array[freq_idx_2_arr(melody_notes[self->mel_idx] + self->key)]);
    // unmute
    SYNC(&obj_dac, DAC_gap, 0);
		// after call for tempo - gap    
    AFTER(USEC(((self->tempo*note_dur[self->mel_idx])-(self->gap_siz*1000))), self, play_song_funct, 1);
  }else{
    // mute
    SYNC(&obj_dac, DAC_gap, 1);
    // update index
    self->mel_idx = (self->mel_idx+1)%32;
    //after call for gap
    AFTER(MSEC(self->gap_siz), self, play_song_funct, 0);
  }
}

void receiver(App *self, int unused) {
  CANMsg msg;
  int bufferValue;
  CAN_RECEIVE(&can0, &msg);
  SCI_WRITE(&sci0, "Can msg: ");
  print("MSG_ID: %c MSG_DAT: ", msg.msgId);
  SCI_WRITE(&sci0, msg.buff);
  SCI_WRITE(&sci0, "\n");
  char id_sw = msg.msgId;

  if(self->conductor_mode == 0){
    // TOOD similar switch as conductor mode
    // m: mutes the DAC\n"
    // h: sets the frequency of the DAC in hz\n"
    // u: increases loop range by 500\n"
    // d: decreases loop range by 500\n"
    // <int>v: sets the volume to <int>\n"
    // <int>b: sets the bpms of the song\n"
    // s: starts playing the song\n"
    // x: stops playing the song\n"
    // <int>k: changes the playing key to <int>\n"
    switch (id_sw) {
      case 'p':
        print("CAN protocol expects the msgId to be one of the following:\n", 0);
        print("When data needs to be provided, it shall be done in str format in the msg:\n\n", 0);
        print("m: mutes the DAC\n", 0);
        print("h: sets the frequency of the DAC in hz\n", 0);
        print("u: increases loop range by 500\n", 0);
        print("d: decreases loop range by 500\n", 0);
        print("<int>v: sets the volume to <int>\n", 0);
        print("<int>b: sets the bpms of the song\n", 0);
        print("s: starts playing the song\n", 0);
        print("x: stops playing the song\n", 0);
        print("<int>k: changes the playing key to <int>\n", 0);
        break;
      case 'm'://mute
        SYNC(&obj_dac, DAC_mute, 0);
        break;
      case 'b': // set bpms
        msg.buff[msg.length] = '\0';
        bufferValue = atoi(msg.buff);
        if(bufferValue < 60){
          print("Minimum bpms is 60\n", 0);
          bufferValue = 60;
        }else if (bufferValue > 240){
          print("Maximun value is 240\n", 0);
          bufferValue = 240;
        }
        
        SYNC(&mel_obj, mel_set_tempo, bufferValue);
        print("Bpms setted to %d\n", bufferValue);
        break;
      case 's': //start the song
        print("Starting to play the song\n", 0);
        SYNC(&mel_obj, Mel_kill, 0);
        ASYNC(&mel_obj, play_song_funct, 0);
        break;
      case 'x':
        SYNC(&mel_obj, Mel_kill, 1);
        print("Stop the song\n", 0);
        break;
      case 'h'://press h to set the freq in heartz
        msg.buff[msg.length] = '\0';
        bufferValue = atoi(msg.buff);
        if (bufferValue<0){
          bufferValue=0;
          print("Minimum freq is 0 heartzs",0);
        }
        SYNC(&obj_dac, DAC_set_freq, bufferValue);
        print("Setting DAC frequency to %d\n", bufferValue);
        break;
      case 'v'://press v to confirm volume change
        msg.buff[msg.length] = '\0';
        bufferValue = atoi(msg.buff);
        if (bufferValue<1){
          bufferValue=1;
          print("Minimum volume is 1",0);
        }else if (bufferValue>200){
          bufferValue=200;
          print("Maximum volume is 200",0);
        }
        print("Volume set to: %d\n", bufferValue);
        SYNC(&obj_dac, DAC_set_vol, bufferValue);
        break;
      case 'k':
        msg.buff[msg.length] = '\0';
        bufferValue = atoi(msg.buff);
        if (bufferValue < -5){
          bufferValue=-5;
          print("Minimum key is -5\n",0);
        }else if (bufferValue>5){
          bufferValue=5;
          print("Maximum key is 5\n",0);
        }

        SYNC(&mel_obj, mel_set_key, bufferValue);
        print("Key: %d\n", bufferValue);
        break;
      default:
        print("Msg id not recognized\n", 0);
        break;
    }
  }
}

void can_write(App *self, int id){
	CANMsg can_msg;
    can_msg.msgId = id & 0x7F;
    can_msg.nodeId = can_node_id;
    can_msg.length = self->str_index;
    for( int i = 0; i <= self->str_index-1; i++){
		can_msg.buff[i] = self->str_buff[i];
	}
    can_msg.buff[self->str_index] = 0;
    self->str_index = 0;
    CAN_SEND(&can0, &can_msg);
}

void reader(App *self, int c) {
  int bufferValue;
  CANMsg can_msg;
  unsigned long long int start_tim, end_tim, global_start_tim;
  unsigned long long int tot_tim;
  unsigned long long int max_tim;  
  //CANMsg can_msg;

  if(VERBOSE){
    if (c == '\n')
      return;
    print("Rcv: '%c'\n", c);
  }
  if(self->can_mode == 1){
    switch (c)
    {
      case 'e':
        self->can_mode = 0;
		if(self->str_index < 1){
          print("At least msgid shall be inputed\n", 0);
        }else{
          can_msg.msgId = (self->str_buff[0]) & 0x7F;
          can_msg.nodeId = can_node_id;
          can_msg.length = self->str_index-1;
          for( int i = 0; i < self->str_index-1; i++){
            can_msg.buff[i] = self->str_buff[i+1];
          }
          can_msg.buff[self->str_index-1] = 0;
          self->str_index = 0;
          CAN_SEND(&can0, &can_msg);
        }	
        break;
      default:
        self->str_buff[self->str_index++] = c;
        break;
    }

  }else if(self->conductor_mode == 1){
    switch (c) {
      case 'p':
        print("p: shows this message\n",0);
        print("c<str>e: shows this message\n",0);
        print("g: change to musician mode\n",0);
        print("m: mutes the DAC\n",0);
        print("h: sets the frequency of the DAC in hz\n",0);
        print("u: increases loop range by 500\n",0);
        print("d: decreases loop range by 500\n",0);
        print("<int>v: sets the volume to <int>\n",0);
        print("<int>b: sets the bpms of the song\n",0);
        print("s: starts playing the song\n",0);
        print("x: stops playing the song\n",0);
        print("<int>k: changes the playing key to <int>\n",0);
        print("q: enables/disables deadlines\n",0);
        print("<int>t: executes an isolated task <int> times and prints max and avg WCET.\n",0);
		can_write(&app, c);
        break;
      case 'g':
        self->conductor_mode = 0;
        SYNC(&mel_obj, Mel_kill, 1);
        print("Now in musician mode\n", 0);
        break;
      case 'c':
        self->can_mode = 1;
        break;
      case 'm'://mute
        SYNC(&obj_dac, DAC_mute, 0);
		can_write(&app, c);
        break;
      case 'b': // set bpms
        self->str_buff[self->str_index] = '\0';
		can_write(&app, c);
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if(bufferValue < 60){
          print("Minimum bpms is 60\n", 0);
          bufferValue = 60;
        }else if (bufferValue > 240){
          print("Maximun value is 240\n", 0);
          bufferValue = 240;
        }
        
        SYNC(&mel_obj, mel_set_tempo, bufferValue);
        print("Bpms setted to %d\n", bufferValue);
        break;
      case 's': //start the song
        print("Starting to play the song\n", 0);
        SYNC(&mel_obj, Mel_kill, 0);
        ASYNC(&mel_obj, play_song_funct, 0);
		can_write(&app, c);
        break;
      case 'x':
        SYNC(&mel_obj, Mel_kill, 1);
        print("Stop the song\n", 0);
		can_write(&app, c);
        break;
      case 'h'://press h to set the freq in heartz
        self->str_buff[self->str_index] = '\0';
		can_write(&app, c);
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if (bufferValue<0){
          bufferValue=0;
          print("Minimum freq is 0 heartzs",0);
        }
        SYNC(&obj_dac, DAC_set_freq, bufferValue);
        print("Setting DAC frequency to %d\n", bufferValue);
        break;
      case 'u'://press u to increase the loop range
        bufferValue = SYNC(&load, add_loop_range, 0);
        print("Increasing loop range to %d\n", bufferValue);
		can_write(&app, c);
        break;
      case 'd'://press d to decreaset the loop range
        bufferValue = SYNC(&load, sub_loop_range, 0);
        print("Decreasing loop range to %d\n", bufferValue);
		can_write(&app, c);
        break;
      case 'v'://press v to confirm volume change
        self->str_buff[self->str_index] = '\0';
		can_write(&app, c);
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if (bufferValue<1){
          bufferValue=1;
          print("Minimum volume is 1",0);
        }else if (bufferValue>200){
          bufferValue=200;
          print("Maximum volume is 200",0);
        }
        print("Volume set to: %d\n", bufferValue);
        SYNC(&obj_dac, DAC_set_vol, bufferValue);
        break;
      case 'k':
        self->str_buff[self->str_index] = '\0';
		can_write(&app, c);
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if (bufferValue < -5){
          bufferValue=-5;
          print("Minimum key is -5\n",0);
        }else if (bufferValue>5){
          bufferValue=5;
          print("Maximum key is 5\n",0);
        }

        SYNC(&mel_obj, mel_set_key, bufferValue);
        print("Key: %d\n", bufferValue);

        // for(int i = 0; i < 32; i++){
        //   print("%d", per_array[freq_idx_2_arr(melody_notes[i] + bufferValue)]);
        //   if(i != 31)
        //     print(", ",0);
        // }
        // print("\n", 0);
        break;
      case 't':
        self->str_buff[self->str_index] = '\0';
		can_write(&app, c);
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        print("Measuring background task exec time for %d loop executions\n", bufferValue);
        tot_tim = 0;
        max_tim=0;
        for(int i = 0; i < bufferValue; i++){
          start_tim = USEC_OF(CURRENT_OFFSET());
          SYNC(&load, measure_empty_loop, 0);
              end_tim = USEC_OF(CURRENT_OFFSET());
              tot_tim += end_tim - start_tim;
          max_tim = MAX(max_tim, end_tim - start_tim);
        }
        print("Max time per loop: %d us.\n", max_tim);
        print("Avg time per loop: %d us.\n", tot_tim/bufferValue);
        
        print("Measuring tone generator exec time for %d loop executions\n", bufferValue);
        
        tot_tim = 0;
        max_tim=0;
        tot_tim = 0;
        global_start_tim=USEC_OF(CURRENT_OFFSET());
        for (int j=0; j<bufferValue; j++){
          start_tim = USEC_OF(CURRENT_OFFSET());
          for(int i = 0; i < 1000; i++){
            measure_tonegen(&obj_dac);
          }
          end_tim = USEC_OF(CURRENT_OFFSET());
          max_tim = MAX(max_tim, end_tim - start_tim);
        }
        tot_tim = USEC_OF(CURRENT_OFFSET()) - global_start_tim;
        print("Max time per task: %d ns.\n", max_tim);
        print("Avg time per task: %d ns.\n", tot_tim/bufferValue);
		can_write(&app, c);
        break;
      case 'q'://enable and disable deadlines
        SYNC(&obj_dac, dac_deadline, 0);
        SYNC(&load, load_deadline, 0);
		can_write(&app, c);
        break;
      default:
        self->str_buff[self->str_index++] = c;
        break;
    }
  }else{ // musician mode
    switch (c) {
      case 'p':
        print("p: shows this message\n",0);
        print("g: change to conductor mode\n",0);
        print("c<str>e: shows this message\n",0);
        break;
      case 'g':
        self->conductor_mode = 1;
        SYNC(&mel_obj, Mel_kill, 1);
        print("Now in  conductor mode\n", 0);
        break;
      case 'c':
        self->can_mode = 1;
        break;
      default:
        self->str_buff[self->str_index++] = c;
        break;
    }
  }
}

void startApp(App *self, int arg) {
  CANMsg msg;

  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  SCI_WRITE(&sci0, "Hello, hello...\n");

  msg.msgId = 1;
  msg.nodeId = can_node_id;
  msg.length = 6;
  msg.buff[0] = 'H';
  msg.buff[1] = 'e';
  msg.buff[2] = 'l';
  msg.buff[3] = 'l';
  msg.buff[4] = 'o';
  msg.buff[5] = 0;
  CAN_SEND(&can0, &msg);

// AFTER(USEC(1300), &load, empty_loop, 0);
  SYNC(&obj_dac, DAC_gap, 1);
  ASYNC(&obj_dac, DAC_wr, 1);
  SYNC(&mel_obj, Mel_kill, 0);
  ASYNC(&mel_obj, play_song_funct, 0);

}

int main() {
	INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
	TINYTIMBER(&app, startApp, 0);
  return 0;
}