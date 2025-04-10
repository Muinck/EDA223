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

const bool VERBOSE = false;
const int  c_nodeId = 15;

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
  // int int_index;
  // int int_count;
  int can_mode;       // app will receive str for can msg
  int conductor_mode; // when conductor_mode == 1 this board controls the other ones
                      // when musician (conductor_mode == 0) plays what it receives

  bool validBoard[8];
  int boardId[8];
  int currentConductor;
  int validSiz;
} App;

typedef struct {
  Object super;
  int volume;
  int muted;
  int mut_print_en;
  int gap;
  int period;
  int play;
} DAC_obj;

typedef struct {
  Object super;
  int tempo; // period in ms
  int gap_siz; // ms //cte
  int key;
  int kill;
  int mel_idx;
  int myModulo; //so the app knows in which modulo to play
  int validSiz;
} Mel_obj;

#define initApp() { initObject(), {}, 0, {}, 0, 0, {1, 0, 0, 0, 0, 0, 0, 0}, {c_nodeId, 0, 0, 0, 0, 0, 0, 0}, 0, 1}
#define initDAC() { initObject(), 3, 0, 1, 0, 500, 0}
#define initload() { initObject(), 1000, 0}
#define initMel() { initObject(), 60000000/120, 50, 0, 0, 0, 0, 1}

void reader(App*, int);
void receiver(App*, int);

App app = initApp();
DAC_obj obj_dac = initDAC();
Mel_obj mel_obj = initMel();
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

void mel_set_tempo(Mel_obj *self, int tempo){
  self->tempo = bpm2tempo(tempo);
}

void mel_set_key(Mel_obj *self, int key){
  self->key = key;
}

void DAC_set_vol(DAC_obj *self, int vol){
  self->volume = vol;
}

void DAC_mute_print_en(DAC_obj *self, int p_en){
  if(self->mut_print_en == 1){
    self->mut_print_en = 0;
    print("DAC mute state printing disabled\n", 0);
  }else{
    self->mut_print_en = 1;
	print("DAC mute state printing enabled\n", 0);
  };
}

void print_muted(DAC_obj *self, int dummy){
	if(self->muted==1){
		if(self->mut_print_en==1){
			print("DAC muted\n", 0);
		}
		AFTER(SEC(5), self, print_muted, 0);
	}
}

void DAC_mute(DAC_obj *self, int dummy){
  if(self->muted == 1){
    self->muted = 0;
    print("DAC unmuted\n", 0);
  }else{
    self->muted = 1;
	print("DAC muted\n", 0);
	AFTER(SEC(5), self, print_muted, 0);
  }
}

void DAC_gap(DAC_obj *self, int gap){
  self->gap = gap;
}

void Mel_kill(Mel_obj *self, int kill){
  self->kill = kill;
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
    SYNC(&obj_dac, DAC_set_period, per_array[freq_idx_2_arr(melody_notes[self->mel_idx%32] + self->key)]);
    // unmute
    if((self->mel_idx % self->validSiz) == self->myModulo){
      SYNC(&obj_dac, DAC_gap, 0);
    }
		// after call for tempo - gap    
    AFTER(USEC(((self->tempo*note_dur[self->mel_idx%32])-(self->gap_siz*1000))), self, play_song_funct, 1);
  }else{
    // mute
    SYNC(&obj_dac, DAC_gap, 1);
    // update index
    self->mel_idx++;
    //after call for gap
    AFTER(MSEC(self->gap_siz), self, play_song_funct, 0);
  }
}

bool inTheList(int id, const int boardId[8], const bool validBoard[8]) {
  for (int i = 0; i < 8; ++i) {
      if (boardId[i] == id && validBoard[i]) {
          return true; 
      }
  }
  return false; // not found
}
int listPos(int id, const int boardId[8], const bool validBoard[8]) {
  for (int i = 0; i < 8; ++i) {
      if (boardId[i] == id && validBoard[i]) {
          return i; 
      }
  }
  return -1; // not found
}
void sortValidBoards(int boardId[8], const bool validBoard[8]) {
  // Step 1: Extract valid boardIds
  int validIds[8];
  int count = 0;
  
  for (int i = 0; i < 8; ++i) {
      if (validBoard[i]) {
          validIds[count++] = boardId[i];
      }
  }

  // Step 2: Insertion sort on validIds
  for (int i = 1; i < count; ++i) {
      int key = validIds[i];
      int j = i - 1;
      while (j >= 0 && validIds[j] > key) {
          validIds[j + 1] = validIds[j];
          j--;
      }
      validIds[j + 1] = key;
  }

  // Step 3: Put sorted validIds back into boardId
  int index = 0;
  for (int i = 0; i < 8; ++i) {
      if (validBoard[i]) {
          boardId[i] = validIds[index++];
      }
  }
}

void setModulo(Mel_obj *self, int newMod){
  self->myModulo = newMod;
}

void setNewSiz(Mel_obj *self, int newSiz){
  self->validSiz = newSiz;
}

// receives messages from CAN
void receiver(App *self, int unused) {
	CANMsg msg;
	int bufferValue;
	CAN_RECEIVE(&can0, &msg);
  char id_sw = msg.msgId;

  if(VERBOSE){ //print receiving stuff
    SCI_WRITE(&sci0, "Can msg: ");
    print("MSG_ID: %c MSG_DAT: ", msg.msgId);
    SCI_WRITE(&sci0, msg.buff);
    SCI_WRITE(&sci0, "\n");
  }

  // While in MUSICIAN mode:
  if(self->conductor_mode == 0){
    // TOOD similar switch as conductor mode
    // <int>B: sets the bpms of the song\n"
    // S: starts playing the song\n"
    // X: stops playing the song\n"
    // <int>K: changes the playing key to <int>\n"
    switch (id_sw) {
      case 'A':
        if(!inTheList(msg.nodeId, self->boardId, self->validBoard)){
          self->validBoard[self->validSiz] = true;
          self->boardId[self->validSiz] = msg.nodeId;
          self->validSiz++;
          sortValidBoards(self->boardId, self->validBoard); // so we now the order that they will play
          //set modulo that this board will play
          SYNC(&mel_obj, setModulo, listPos(c_nodeId, self->boardId, self->validBoard));
          SYNC(&mel_obj, setNewSiz, self->validSiz);
          print("New Board added with nodeId: %d\n", msg.nodeId);
          if(VERBOSE){
            print("Our modulo is now: %d\n", listPos(c_nodeId, self->boardId, self->validBoard));
            print("Total number of boards: %d\n",self->validSiz);
          }
        }
        break;
      case 'C':
        print("%d nodeId claimed conductor\n", msg.nodeId);
        self->currentConductor = msg.nodeId;
        self->conductor_mode = 0;
        break;
      case 'h':
        print("CAN protocol expects the msgId to be one of the following:\n", 0);
        print("When data needs to be provided, it shall be done in str format in the msg:\n\n", 0);
        print("h: shows this message\n", 0);
        print("<int>B: sets the bpms of the song\n", 0);
        print("S: starts playing the song\n", 0);
        print("X: stops playing the song\n", 0);
        print("<int>K: changes the playing key to <int>\n", 0);
        break;
      case 'B': // set bpms
        if(msg.nodeId == self->currentConductor){
          msg.buff[msg.length] = '\0';
          bufferValue = atoi((char*)msg.buff);
          if(bufferValue < 30){
            print("Minimum bpms is 30\n", 0);
            bufferValue = 30;
          }else if (bufferValue > 300){
            print("Maximun value is 300\n", 0);
            bufferValue = 300;
          }
          
          SYNC(&mel_obj, mel_set_tempo, bufferValue);
          print("Bpms setted to %d\n", bufferValue);
        }else{
          print("Musician trying to set BMPs, ignored\n", 0);
        }
        break;
      case 'P': //start the song
        if(msg.nodeId == self->currentConductor){
          print("Starting to play the song\n", 0);
          SYNC(&mel_obj, Mel_kill, 0);
          ASYNC(&mel_obj, play_song_funct, 0);
          break;
        }else{
          print("Musician trying to start the song, ignored\n", 0);
        }
      case 'X':
        if(msg.nodeId == self->currentConductor){
          SYNC(&mel_obj, Mel_kill, 1);
          print("Stop the song\n", 0); 
        }else{
          print("Musician trying to stop the song, ignored\n", 0);
        }
        break;
      case 'K':
        if(msg.nodeId == self->currentConductor){
          msg.buff[msg.length] = '\0';
          bufferValue = atoi((char*)msg.buff);
          if (bufferValue < -5){
            bufferValue=-5;
            print("Minimum key is -5\n",0);
          }else if (bufferValue>5){
            bufferValue=5;
            print("Maximum key is 5\n",0);
          }

          SYNC(&mel_obj, mel_set_key, bufferValue);
          print("Key: %d\n", bufferValue); 
        }else{
          print("Musician trying to set KEY, ignored\n", 0);
        }
        break;
      default:
        print("Msg id not recognized\n", 0);
        break;
    }
  }else{
    switch (id_sw) {
      case 'A':
        if(!inTheList(msg.nodeId, self->boardId, self->validBoard)){
          self->validBoard[self->validSiz] = true;
          self->boardId[self->validSiz] = msg.nodeId;
          self->validSiz++;
          sortValidBoards(self->boardId, self->validBoard); // so we now the order that they will play
          //set modulo that this board will play
          SYNC(&mel_obj, setModulo, listPos(c_nodeId, self->boardId, self->validBoard));
          print("New Board added with nodeId: %d\n", msg.nodeId);
          if(VERBOSE){
            print("Our modulo is now: %d\n", listPos(c_nodeId, self->boardId, self->validBoard));
            print("Total number of boards: %d\n",self->validSiz);
          }
        }
        break;
      case 'C':
        //if in conductor change back to mussician and give them control
        if(self->conductor_mode != 0){
          self->conductor_mode = 0;
          self->currentConductor = msg.nodeId;
          print("Giving conductor control, now in MUSICIAN mode\n", 0);
        }
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
  can_msg.nodeId = c_nodeId;
  can_msg.length = self->str_index;
  for(int i = 0; i <= self->str_index; i++){
		can_msg.buff[i] = self->str_buff[i];
	}
    self->str_index = 0;
    CAN_SEND(&can0, &can_msg);
}

// Keyboard 
void reader(App *self, int c) {
  int bufferValue;
  CANMsg can_msg;
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

  }else if(self->conductor_mode == 1){//conductor mode
		switch (c) {
			case 'h':
				print("h: shows this message\n",0);
				// print("G: change to musician mode\n",0); //For now the only way to change back is by other board to kick you out
				print("<int>V: sets the volume to <int>\n",0);
				print("<int>B: sets the bpms of the song\n",0);
        print("c: debug can mode\n",0);
				print("P: starts playing the song\n",0);
        print("T: mutes/unmutes song\n",0);
				print("X: stops playing the song\n",0);
				print("<int>K: changes the playing key to <int>\n",0);
				break;
			// case 'G':
			// 	self->conductor_mode = 0;
			// 	SYNC(&mel_obj, Mel_kill, 1);
			// 	print("Now in musician mode\n", 0);
			// 	break;
      case 'c':
			  self->can_mode = 1;
			  break;
      case 'T'://mute
        SYNC(&obj_dac, DAC_mute, 0);
        break;
      case 'I'://enable or disabled mute state printing
        SYNC(&obj_dac, DAC_mute_print_en, 0);
        break;
			case 'B': // set bpms
				self->str_buff[self->str_index] = '\0';
				can_write(&app, c);
				self->str_index = 0;
				bufferValue = atoi(self->str_buff);
				if(bufferValue < 30){
				  print("Minimum bpms is 30\n", 0);
				  bufferValue = 30;
				}else if (bufferValue > 300){
				  print("Maximun value is 300\n", 0);
				  bufferValue = 300;
				}			
				SYNC(&mel_obj, mel_set_tempo, bufferValue);
				print("Bpms setted to %d\n", bufferValue);
				break;
			case 'P': //start the song
				print("Starting to play the song\n", 0);
				SYNC(&mel_obj, Mel_kill, 0);
				ASYNC(&mel_obj, play_song_funct, 0);
				can_write(&app, c);
				break;
			case 'X':
				SYNC(&mel_obj, Mel_kill, 1);
				print("Stop the song\n", 0);
				can_write(&app, c);
				break;
			case 'V'://press v to confirm volume change
				self->str_buff[self->str_index] = '\0';
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
			case 'K':
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
				break;
			default:
				self->str_buff[self->str_index++] = c;
				break;
		}
	}else{ // musician mode
		switch (c) {
		  case 'h':
        print("h: shows this message\n",0);
        print("C: claims conductor mode\n",0);
        print("c: debug can mode\n",0);
        print("T: mutes/unmutes song\n",0);
        print("I: enables/disables mute state printing\n",0);
        print("<int>V: sets the volume to <int>\n",0);
        break;
		  case 'C':
        self->conductor_mode = 1;        
        can_msg.msgId = 'C';
        can_msg.nodeId = c_nodeId;
        can_msg.length = 0;
        CAN_SEND(&can0, &can_msg);
        print("Conductor mode claimed\n", 0);
        break;
      case 'c':
        self->can_mode = 1;
        break;
      case 'T'://mute
        SYNC(&obj_dac, DAC_mute, 0);
        break;
      case 'I'://enable or disabled mute state printing
        SYNC(&obj_dac, DAC_mute_print_en, 0);
        break;
      case 'V'://press v to confirm volume change
        self->str_buff[self->str_index] = '\0';
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
      default:
        self->str_buff[self->str_index++] = c;
        break;
		}
	}
}

void send_ping(App *self, int dummy){
	CANMsg can_msg;
  can_msg.msgId = 'A';
  can_msg.nodeId = c_nodeId;
  can_msg.length = 0;
  CAN_SEND(&can0, &can_msg);

  // send ping every 100 ms all time
  AFTER(MSEC(100), &self, send_ping, 0);
}

void startApp(App *self, int arg) {
  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  SCI_WRITE(&sci0, "Hello, hello...\n");

  SYNC(&obj_dac, DAC_gap, 1);
  ASYNC(&obj_dac, DAC_wr, 1);
  SYNC(&mel_obj, Mel_kill, 0);
  AFTER(USEC(100), &app, send_ping, 0);
}

int main() {
	INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
	TINYTIMBER(&app, startApp, 0);
  return 0;
}