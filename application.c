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
  Msg removalTimers[8];
  int currentConductor;
  int validSiz;
  int failureType;
  int failureMode;
  Msg failureTimer;
  int nodeId;
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
  int mel_mod;
  int myModulo; //so the app knows in which modulo to play
  int validSiz;
  int send_sync;
  bool validBoard[8];
  int boardId[8];
} Mel_obj;

#define initApp() { initObject(), {}, 0, {}, 0, 0, {1, 0, 0, 0, 0, 0, 0, 0}, {c_nodeId, 0, 0, 0, 0, 0, 0, 0}, {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}, 0, 1, 0, 0, NULL, c_nodeId}
#define initDAC() { initObject(), 3, 0, 1, 0, 500, 0}
#define initload() { initObject(), 1000, 0}
#define initMel() { initObject(), 60000000/120, 50, 0, 0, 0, 0, 0, 1, 0, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}}

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

void sortValidBoards(int boardId[8], bool validBoard[8], Msg removalTimers[8]) {
  // Temporary arrays to hold valid board IDs
  int validIds[8];
  Msg valid_timers[8];
  int count = 0;

  for(int i = 0; i < 8; i++){
    if(self->removalTimers[i] != NULL && validBoard[i]){
      ABORT(self->removalTimers[i]);
      self->removalTimers[i] = NULL;
    }
  }

  // Collect valid boardIds
  for (int i = 0; i < 8; i++) {
      if (validBoard[i]) {
        valid_timers[count] = removalTimers[i];
        validIds[count++] = boardId[i];
      }
  }

  // Simple insertion sort on validIds
  for (int i = 1; i < count; i++) {
      int key = validIds[i];
      int msg = valid_timers[i];
      int j = i - 1;
      while (j >= 0 && validIds[j] > key) {
          validIds[j + 1] = validIds[j];
          valid_timers[j + 1] = valid_timers[j];
          j--;
      }
      validIds[j + 1] = key;
      valid_timers[j + 1] = msg;
  }

  // Fill boardId and validBoard arrays with sorted valid ones first
  for (int i = 0; i < count; i++) {
      boardId[i] = validIds[i];
      removalTimers[i] = valid_timers[i];
      validBoard[i] = true;
  }

  // Mark the rest as invalid
  for (int i = count; i < 8; i++) {
      validBoard[i] = false;
      removalTimers[i] = NULL;
  }
}

void setNewSiz(Mel_obj *self, int newSiz){
  self->validSiz = newSiz;
}

void setModulo(Mel_obj *self, int newMod){
  self->myModulo = newMod;
}

void removeBoard(App *self, int arg) {
  int pos = listPos(arg, self->boardId, self->validBoard);
  if(pos == -1){
    print("Board %d trying to be removed not in the list, ignoring\n", arg);
  }
  
  //Inform we lost the board
  CANMsg can_msg;
  can_msg.msgId = 'L';
  can_msg.nodeId = self->nodeId;
  can_msg.length = 1;
  can_msg.buff[0] = (uchar) self->boardId[pos];
  CAN_SEND(&can0, &can_msg);

  if(VERBOSE){
    print("info remove board nodeId: %d\n", self->boardId[pos]);
  }

  if (pos >= 0 && pos < self->validSiz && self->validBoard[pos]) {
      print("Board %d timed out and was removed.\n", self->boardId[pos]);
      if(self->validBoard[pos] != NULL){
        ABORT(self->removalTimers[pos]);
        self->removalTimers[pos] = NULL;
      }
      self->validBoard[pos] = false;

      // Optional: compact board list
      self->validSiz--;
      sortValidBoards(self->boardId, self->validBoard, self->removalTimers);
      SYNC(&mel_obj, setNewSiz, self->validSiz);
      SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
      if(VERBOSE){
        print("Our modulo is now: %d\n", listPos(self->nodeId, self->boardId, self->validBoard));
        print("Total number of boards: %d\n",self->validSiz);
      }
  }

  // Clear the reference to the fired Msg
  self->removalTimers[pos] = NULL;
}

void* get_playing(Mel_obj *self, int unused) {
  int ret = 0;
  if(self->kill == 0){
    ret = 1;
  }
  return (void*)(intptr_t)(ret);
}

void* get_tempo(Mel_obj *self, int unused) {
  int ret = 60000000 / self->tempo;
  return (void*)(intptr_t)(ret);
}

void* get_key(Mel_obj *self, int unused) {
  int ret = self->key;
  return (void*)(intptr_t)(ret);
}

void mel_add_sync_board(bool validBoard[8], int boardId[8], int nodeId) {
  for (int i = 0; i < 8; i++) {
      if (validBoard[i] && boardId[i] == nodeId) {
          // Already in the list
          return;
      }
  }
  // Add to first free slot
  for (int i = 0; i < 8; i++) {
      if (!validBoard[i]) {
          boardId[i] = nodeId;
          validBoard[i] = true;
          return;
      }
  }
}

void add_sync_board(Mel_obj *self, int nodeId) {
  mel_add_sync_board(self->validBoard, self->boardId, nodeId);
}

void send_sync(Mel_obj *self, int nodeId) {
  self->send_sync = 1;
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

void* get_failure(App *self, int dummy){
  return (void*)(intptr_t)(self->failureMode);
}

void syncModulo(Mel_obj *self, int newMod){
  self->mel_mod = newMod;
}
void syncMel_idx(Mel_obj *self, int newIdx){
  self->mel_idx = newIdx;
}

void app_add_board(App *self, int nodeId){
  for (int i = 0; i < 8; i++) {
    if (self->validBoard[i] && self->boardId[i] == nodeId) {
        // Already in the list
        return;
    }
  }
  self->validBoard[self->validSiz] = true;
  self->boardId[self->validSiz] = nodeId;
  self->validSiz++;
  sortValidBoards(self->boardId, self->validBoard, self->removalTimers); // so we now the order that they will play
  //set modulo that this board will play
  SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
  print("New Board added with nodeId: %d\n", nodeId);
}

void* get_nodeId(App *self, int dummy){
  int ret = self->nodeId;
  return (void*)(intptr_t)(ret);
}

void play_song_funct(Mel_obj *self, int in){
  // 0 -> note, 1 -> gap

  if(self->kill == 1){
    // mute
    SYNC(&obj_dac, DAC_gap, 1);
    self->mel_idx = 0;
    self->mel_mod = 0;
    return;
  }
  if(in == 0){
    //TODO sending it here is a problem, maybe better if we do it when starting the gap
    if (self->send_sync == 1) {
      print("Sending all sync commands...\n", 0);
      for (int i = 0; i < 8; ++i) {
          if (self->validBoard[i]) {
              CANMsg can_msg;
              can_msg.msgId = 'S';
              can_msg.nodeId = SYNC(&app, get_nodeId, 0);  // sender's nodeId
              can_msg.length = 3;
              can_msg.buff[0] = (unsigned char)(self->boardId[i]);  // recipient nodeId
              can_msg.buff[1] = (unsigned char)(self->mel_idx);          // current note index
              can_msg.buff[2] = (unsigned char)(self->mel_mod);          // modulo position
  
              // add to our valid list
              SYNC(&app, app_add_board, self->boardId[i]);

              CAN_SEND(&can0, &can_msg);
              
              if(self->validBoard[i] != NULL){
                ABORT(self->removalTimers[i]);
                self->removalTimers[i] = NULL;
              }
              self->validBoard[i] = false;

              if(VERBOSE){
                print("Sending sync command to %d\n", self->boardId[i]);
              }
          }
      }
      self->send_sync = 0;       // Reset sync flag
    }
  
    int failure = (int)(intptr_t)SYNC(&app, get_failure, 0);
    // set new tone
    SYNC(&obj_dac, DAC_set_period, per_array[freq_idx_2_arr(melody_notes[self->mel_idx] + self->key)]);
    // unmute
    if(self->mel_mod == self->myModulo && failure == 0){
      SYNC(&obj_dac, DAC_gap, 0);
    }
    if(failure == 1){
      print("Not playing because failure = 1\n", 0);
    }
		// after call for tempo - gap    
    AFTER(USEC(((self->tempo*note_dur[self->mel_idx])-(self->gap_siz*1000))), self, play_song_funct, 1);
  }else{
    // mute
    SYNC(&obj_dac, DAC_gap, 1);
    // update index
    self->mel_idx = (self->mel_idx+1) % 32;
    self->mel_mod = (self->mel_mod+1) % self->validSiz;
    //after call for gap
    AFTER(MSEC(self->gap_siz), self, play_song_funct, 0);
  }
}

int int_to_str(int value, unsigned char buff[8]) {
  int i = 0;
  int isNegative = 0;

  if (value < 0) {
      isNegative = 1;
      value = -value;
  }

  if (value == 0) {
      buff[i++] = '0';
  } else {
      unsigned char temp[8];
      int j = 0;

      while (value > 0 && j < 7) {  // max 7 digits to leave room for null
          temp[j++] = '0' + (value % 10);
          value /= 10;
      }

      if (isNegative && i < 7) {
          buff[i++] = '-';
      }

      while (j > 0 && i < 8) {
          buff[i++] = temp[--j];
      }
  }

  // Null-terminate only if there's space (optional)
  if (i < 8) {
      buff[i] = '\0';
  } else {
      buff[7] = '\0'; // make sure it's terminated
  }

  return i; // number of characters before null terminator
}

// receives messages from CAN
void receiver(App *self, int unused) {
	CANMsg msg;
	int bufferValue;
	CAN_RECEIVE(&can0, &msg);
  char id_sw = msg.msgId;

  // if(VERBOSE){ //print receiving stuff
  //   SCI_WRITE(&sci0, "Can msg: ");
  //   print("MSG_ID: %c MSG_DAT: ", msg.msgId);
  //   SCI_WRITE(&sci0, msg.buff);
  //   SCI_WRITE(&sci0, "\n");
  // }

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
          sortValidBoards(self->boardId, self->validBoard, self->removalTimers); // so we now the order that they will play
          //set modulo that this board will play
          SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
          SYNC(&mel_obj, setNewSiz, self->validSiz);
          print("New Board added with nodeId: %d\n", msg.nodeId);
          if(VERBOSE){
            print("Our modulo is now: %d\n", listPos(self->nodeId, self->boardId, self->validBoard));
            print("Total number of boards: %d\n",self->validSiz);
          }
        }
        break;
      case 'L':
        if((int)msg.buff[0] == self->nodeId){
          SYNC(&mel_obj, Mel_kill, 1); //stop playing
          //empty all the valid list except for us
          self->validSiz = 1;
          self->validBoard[0] = true;
          for (int i = 1; i < 8; i++) {
            if(self->validBoard[i] != NULL){
              ABORT(self->removalTimers[i]);
              self->removalTimers[i] = NULL;
            }
            self->validBoard[i] = false;
          }
          self->boardId[0] = self->nodeId;
          for(int i = 0; i < 8; i++){
            if(self->removalTimers[i] != NULL){
              ABORT(self->removalTimers[i]);
              self->removalTimers[i] = NULL;
            }
          }
          //set modulo that this board will play
          SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
          SYNC(&mel_obj, setNewSiz, self->validSiz);
          print("All boards removed from active list\n", 0);
          if(VERBOSE){
            print("Our modulo is now: %d\n", listPos(self->nodeId, self->boardId, self->validBoard));
            print("Total number of boards: %d\n",self->validSiz);
          }
        }else{
          if(inTheList(((int)msg.buff[0]), self->boardId, self->validBoard)){
            int rm_pos = listPos((int)msg.buff[0], self->boardId, self->validBoard);
            if(self->validBoard[rm_pos] != NULL){
              ABORT(self->removalTimers[rm_pos]);
              self->removalTimers[rm_pos] = NULL;
            }
            self->validBoard[rm_pos] = false;
            self->validSiz--;
            sortValidBoards(self->boardId, self->validBoard, self->removalTimers); // so we now the order that they will play
            //set modulo that this board will play
            SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
            SYNC(&mel_obj, setNewSiz, self->validSiz);
            print("Board removed with nodeId: %d\n", ((int)msg.buff[0]));
            if(VERBOSE){
              print("Our modulo is now: %d\n", listPos(self->nodeId, self->boardId, self->validBoard));
              print("Total number of boards: %d\n",self->validSiz);
            }
          }
        }
        break;
      case 'S':
        if(VERBOSE){
          print("Got S msg from NodeId %d\n", msg.nodeId);
          print("DAT: destination nodeId %d", (int)msg.buff[0]);
          print(" new mel_idx %d", (int)msg.buff[1]);
          print(" new mel_mod %d\n", (int)msg.buff[2]);
        }
        if((int)msg.buff[0] == self->nodeId){ //if is going to me, update the stuff
          int new_mel_idx = (int)msg.buff[1];
          int new_mel_mod = (int)msg.buff[2];
          // set module and melody index
          SYNC(&mel_obj, syncMel_idx, new_mel_idx);
          SYNC(&mel_obj, syncModulo, new_mel_mod);

          //and start playing
          SYNC(&mel_obj, Mel_kill, 0);
          ASYNC(&mel_obj, play_song_funct, 0);
        }
        break;
      case 'C':
        print("%d nodeId claimed conductor\n", (15-msg.nodeId));
        self->currentConductor = (15-msg.nodeId);
        self->conductor_mode = 0;
        for(int i = 0; i < 8; i++){
          if(self->removalTimers[i] != NULL){
            ABORT(self->removalTimers[i]);
            self->removalTimers[i] = NULL;
          }
        }
        break;
      case 'h':
        print("CAN protocol expects the msgId to be one of the following:\n", 0);
        print("When data needs to be provided, it shall be done in str format in the msg:\n\n", 0);
        print("h: shows this message\n", 0);
        print("<int>B: sets the bpms of the song\n", 0);
        print("L: byte(0)=nodeId removes the board from the available list\n", 0);
        print("S: sync message with nodeId, mel_idx and mel_mod info\n", 0);
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
    int music_playing = SYNC(&mel_obj, get_playing, 0);
    switch (id_sw) {
      case 'A':
        if(!inTheList(msg.nodeId, self->boardId, self->validBoard)){
          CANMsg can_msg;
          can_msg.msgId = 'C';
          can_msg.nodeId = (15-self->nodeId);
          can_msg.length = 0;
          CAN_SEND(&can0, &can_msg);
          can_msg.msgId = 'K';
          can_msg.nodeId = self->nodeId;
          can_msg.length = int_to_str(SYNC(&mel_obj, get_key, 0), can_msg.buff);
          CAN_SEND(&can0, &can_msg);
          can_msg.msgId = 'B';
          can_msg.nodeId = self->nodeId;
          can_msg.length = int_to_str(SYNC(&mel_obj, get_tempo, 0), can_msg.buff);
          CAN_SEND(&can0, &can_msg);

          if(music_playing){
            // add it later with the sync comman
            SYNC(&mel_obj, add_sync_board, msg.nodeId);
            AFTER(MSEC(250), &mel_obj, send_sync, msg.nodeId);
            if(VERBOSE){
              print("Commanding sync command\n", 0);
            }
          }
          
          self->validBoard[self->validSiz] = true;
          self->boardId[self->validSiz] = msg.nodeId;
          self->validSiz++;
          sortValidBoards(self->boardId, self->validBoard, self->removalTimers); // so we now the order that they will play
          //set modulo that this board will play
          SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
          SYNC(&mel_obj, setNewSiz, self->validSiz);
          print("New Board added with nodeId: %d\n", msg.nodeId);
          if(VERBOSE){
            print("Our modulo is now: %d\n", listPos(self->nodeId, self->boardId, self->validBoard));
            print("Total number of boards: %d\n",self->validSiz);
          }
        }else {
          int pos = listPos(msg.nodeId, self->boardId, self->validBoard);
          if (pos >= 0 && pos < 8) {
            // Cancel previous pending removal if any
            if (self->removalTimers[pos] != NULL) {
              ABORT(self->removalTimers[pos]);
            }
            // Schedule new removal and save the Msg
            self->removalTimers[pos] = AFTER(MSEC(250), self, removeBoard, msg.nodeId);
          }
        }
        break;
      case 'C':
        //if in conductor change back to mussician and give them control
        if(self->conductor_mode != 0){
          self->conductor_mode = 0;
          self->currentConductor = (15-msg.nodeId);
          print("Giving conductor control, now in MUSICIAN mode\n", 0);
          
          for(int i = 0; i < 8; i++){
            if(self->removalTimers[i] != NULL){
              ABORT(self->removalTimers[i]);
              self->removalTimers[i] = NULL;
            }
          }
        }
        break;
      default:
        print("Msg id not recognized\n", 0);
        break;
    }
  }
}

void send_ping(App *self, int dummy){
	CANMsg can_msg;
  if(self->failureMode == 0){
    can_msg.msgId = 'A';
    can_msg.nodeId = self->nodeId;
    can_msg.length = 0;
    int can_ret = CAN_SEND(&can0, &can_msg);
    if(can_ret != 0 && self->conductor_mode == 0 && SYNC(&mel_obj, get_playing, 0)){
      print("Im an alone musician, so i stop playing :(\n", 0);
      SYNC(&mel_obj, Mel_kill, 1);
    }

    if(dummy%10 == 0 && VERBOSE){
      print("Sending ping\n", 0);
    }

    // send ping every 100 ms all time
    AFTER(MSEC(100), self, send_ping, ++dummy);
  }
}

void out_failure(App *self, int id){
  self->failureMode = 0;

  AFTER(USEC(1), self, send_ping, 0);
  print("Comming out of failure mode\n", 0);
}

void can_write(App *self, int id){
	CANMsg can_msg;
  can_msg.msgId = id & 0x7F;
  can_msg.nodeId = self->nodeId;
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
				print("R: resets the configuration (bpm and key)\n",0);
				print("P: starts playing the song\n",0);
				print("M: print board status\n",0);
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
      case 'M':
        print("Active boards rank sorted:\n", 0);
        for(int i = 0; i < self->validSiz; i++){
          print("%d ", self->boardId[i]);
        }
        print("\n", 0);
        break;
      case 'R': // set bpms
        SYNC(&mel_obj, mel_set_tempo, 120);
        SYNC(&mel_obj, mel_set_key, 0);
        print("Bpms and set tempo resetted, 120 and 0 values respectivly\n", 0);
        can_msg.msgId = 'B';
        can_msg.nodeId = self->nodeId;
        can_msg.length = 3;
        can_msg.buff[0] = '1';
        can_msg.buff[1] = '2';
        can_msg.buff[2] = '0';
        can_msg.buff[3] = '\0';
        CAN_SEND(&can0, &can_msg);
        can_msg.msgId = 'K';
        can_msg.nodeId = self->nodeId;
        can_msg.length = 1;
        can_msg.buff[0] = '0';
        can_msg.buff[1] = '\0';
        CAN_SEND(&can0, &can_msg);
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
    int random = (rand()%20)+10;
		switch (c) {
		  case 'h':
        print("h: shows this message\n",0);
        print("C: claims conductor mode\n",0);
        print("c: debug can mode\n",0);
        print("T: mutes/unmutes song\n",0);
				print("M: print board status\n",0);
				print("<int>N: change board nodeId\n",0);
        print("<int>F: enters the specified error mode\n", 0);
        print("G: comes out of the error mode\n", 0);
        print("I: enables/disables mute state printing\n",0);
        print("<int>V: sets the volume to <int>\n",0);
        break;  
      case 'M':
        print("Active boards rank sorted:\n", 0);
        for(int i = 0; i < self->validSiz; i++){
          print("%d ", self->boardId[i]);
        }
        print("\n", 0);
        break;     
      case 'N': // set nodeId
        self->str_buff[self->str_index] = '\0';
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if(bufferValue < 0){
          print("Node id should be positive\n", 0);
          bufferValue = 2;
        }else if (bufferValue > 15){
          print("Maximun value is 15\n", 0);
          bufferValue = 15;
        }			
        self->nodeId = bufferValue;

        //update the boards
        self->validSiz = 1;
        self->validBoard[0] = true;
        for (int i = 1; i < 8; i++) {
          self->validBoard[i] = false;
        }
        self->boardId[0] = self->nodeId;
        for(int i = 0; i < 8; i++){
          if(self->removalTimers[i] != NULL){
            ABORT(self->removalTimers[i]);
            self->removalTimers[i] = NULL;
          }
        }
        sortValidBoards(self->boardId, self->validBoard, self->removalTimers); // so we now the order that they will play
        //set modulo that this board will play
        SYNC(&mel_obj, setModulo, listPos(self->nodeId, self->boardId, self->validBoard));
        SYNC(&mel_obj, setNewSiz, self->validSiz);

        print("NodeIde setted to %d\n", bufferValue);
        break;
      case 'F':
        self->str_buff[self->str_index] = '\0';
        self->str_index = 0;
        bufferValue = atoi(self->str_buff);
        if(bufferValue == 1){
          SYNC(&mel_obj, Mel_kill, 1);
          self->failureType = bufferValue;
          self->failureMode = 1;
          print("F1 failure mode activated\n", 0);
        }else if (bufferValue == 2){
          SYNC(&mel_obj, Mel_kill, 1);
          self->failureType = bufferValue;
          self->failureMode = 1;
          self->failureTimer = AFTER(SEC(random), self, out_failure, 0);
          print("F2 failure mode activated, comming in %d sec\n", random);
        }else{ // error not recognized
          print("F%d failure mode not recognized\n", bufferValue);
        }
        break;
      case 'G':
        self->failureMode = 0;
        if(self->failureType == 2){
          ABORT(self->failureTimer);
        }
        AFTER(USEC(1), self, send_ping, 0);
        print("Comming out of failure mode\n", 0);
        break;
		  case 'C':
        self->conductor_mode = 1;        
        can_msg.msgId = 'C';
        can_msg.nodeId = 15 - self->nodeId;
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

void startApp(App *self, int arg) {
  CAN_INIT(&can0);
  SCI_INIT(&sci0);
  SCI_WRITE(&sci0, "Hello, hello...\n");

  SYNC(&obj_dac, DAC_gap, 1);
  ASYNC(&obj_dac, DAC_wr, 1);
  SYNC(&mel_obj, Mel_kill, 1);
  AFTER(MSEC(250), self, send_ping, 0);
}

int main() {
	INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
	TINYTIMBER(&app, startApp, 0);
  return 0;
}