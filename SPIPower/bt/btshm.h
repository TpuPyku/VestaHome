// btshm.h - the Bluetooth proxy's shared memory (FLC_BLUETOOTH_SDK.dll proxy <-> VestaHome; VestaHome includes
// this very file).
//
// The proxy lives in the stock shell (G3NavHMI loads NCL_BT.DLL -> FLC_BLUETOOTH_SDK.dll). Flc_Bluetooth_Init(cb) is
// wrapped: every event of the stack (cb(int* ev), ev[0] = the event's code) is published here before the shell's own
// callback gets it, and VestaHome's commands are carried out by a thread of the proxy, in the shell's process (a
// second copy of the SDK in VestaHome would register the profiles again and break the shell's Bluetooth).
//
// Events (ev[0]; Ghidra 2026-10-18 on NCL_BT.DLL's handler, which switches on ev[0] - 1; PROJECT.md):
//   0x01 the stack is ready              0x0A the connected device: address "A4:E2:..." (text) at byte 4, UTF-8 name
//   0x08 one paired device, as 0x0A               at byte 0x16 (HU 2026-10-18)
//   0x0E the caller's number: ev[1] 1 = there is one, its text at byte 9
//   0x10 a call's state: ev[2] the line (1 / 2), ev[1] 0 none, 1 incoming, 2 dialing, 3 ringing there, 4 active,
//        5 held, 6 waiting                      0x14 the network operator's name at byte 4
//   0x11 HF: ev[1] 2 connected, 0 not    0x12 HF audio: ev[1] 1 on, 0 off
//   0x15 phone battery (char)ev[1]       0x16 phone signal (char)ev[1]
//   0x2B A2DP: ev[1] 0 disconnected, 1 connected, 2 streaming
//   0x2C play: ev[1] length ms, ev[2] position ms, ev[3] 1 playing / 2 paused (every 2 s while playing)
//                                        0x2E track info: ev[1] items, item i = ev[2 + i*0x42] AVRCP attribute
//   0x30 AVRCP: ev[1] 1 connected, 0 not      (1 title, 2 artist, 3 album, 4 track no., 5 tracks, 6 genre,
//                                              7 duration ms), its UTF-8 text at ev + 4 + i*0x42
// State: written under a sequence lock (seq odd while writing); a reader copies it between two equal even seqs.
#ifndef BTSHM_H
#define BTSHM_H

#define BT_SHM_NAME   L"VestaBT4"       // v4 (2026-10-20): 16 paired, search / pair / unpair (v3: calls, rewind)
#define BT_CMD_EVENT  L"VestaBT4_Cmd"   // set by VestaHome after writing a command
#define BT_SHM_MAGIC  0x54425456        // "VTBT"
#define BT_EV_SLOTS   64
#define BT_EV_DATA    64                // the first bytes of each event, for the log and later decoding
#define BT_TEXT       128               // UTF-8 bytes kept of a name / title
#define BT_PAIRED     16                // paired devices kept

enum { BT_CMD_NONE, BT_CMD_PLAYPAUSE, BT_CMD_PLAY, BT_CMD_PAUSE, BT_CMD_NEXT, BT_CMD_PREV, BT_CMD_MEDIAINFO,
       BT_CMD_PLAYSTATUS, BT_CMD_AVSTATE, BT_CMD_FF_PRESS, BT_CMD_FF_RELEASE, BT_CMD_REW_PRESS, BT_CMD_REW_RELEASE,
       BT_CMD_DIAL /* cmdText */, BT_CMD_ANSWER, BT_CMD_END,
       BT_CMD_CONNECT /* cmdText: "A4:E2:..." - the phone, as the shell connects one: Hf_Connect(addr, 0) +
                         Av_Connect() */,
       BT_CMD_AVCONNECT /* Av_Connect(): the music of the phone connected (the shell's, 2 s after asking) */,
       BT_CMD_PLAYON /* Av_Mute(0) + Av_Play(): the shell's BT music "play" */,
       BT_CMD_SEARCH /* Conn_Search(): the devices in sight come as events, the address text at byte 4 */,
       BT_CMD_SEARCHSTOP /* Conn_Cancel_Search() */,
       BT_CMD_PAIR /* cmdText: "A4:E2:87:98:D6:9F 1234" - Conn_Pair(addr) with this PIN; the stack's request for it
                      (an event with the address at byte 4) is answered by the proxy, then the shell's own PIN is
                      put back */,
       BT_CMD_LISTPAIRED /* Conn_List_History(): the paired list again (0x08 each, 0x09 the end) */,
       BT_CMD_UNPAIR /* cmdText: the address - Conn_Delete_Paired(addr), then the list again */ };
enum { BT_CALL_NONE, BT_CALL_INCOMING, BT_CALL_DIALING, BT_CALL_ALERTING, BT_CALL_ACTIVE, BT_CALL_HELD, BT_CALL_WAITING };

struct BtEvent {
    volatile LONG seq;                  // = the writer's n when the slot is complete
    DWORD         tick;
    int           code;                 // ev[0]
    unsigned char data[BT_EV_DATA];     // the event from ev[1] on
};

struct BtState {
    int   ready;                        // 0x01 seen
    int   av;                           // 0x2B ev[1]: 0 / 1 / 2, -1 none yet
    int   avrcp, hf, hfAudio;           // 0x30, 0x11, 0x12 ev[1], -1 none yet
    int   lengthMs, posMs, playing;     // 0x2C: playing 1, paused 2, 0 none yet
    int   battery, signal;              // 0x15, 0x16, -1 none yet
    char  devAddr[18];                  // 0x0A, "A4:E2:87:98:D6:9F"
    char  devName[BT_TEXT];
    int   paired;                       // 0x08: up to BT_PAIRED, by address
    char  pairedAddr[BT_PAIRED][18];
    char  pairedName[BT_PAIRED][48];
    char  title[BT_TEXT], artist[BT_TEXT], album[BT_TEXT];   // 0x2E
    int   trackNo, trackCount, durationMs;
    int   call[2];                      // 0x10, per line: BT_CALL_*
    char  callNumber[32];               // 0x0E
    char  operatorName[32];             // 0x14
    DWORD callTick;                     // when a call's state last changed
    DWORD avTick, playTick, mediaTick, devTick;
};

struct BtShm {
    DWORD         magic;
    DWORD         version;              // 3
    volatile LONG seq;                  // the state's sequence lock
    volatile LONG evSeq;                // events written so far
    volatile LONG attach;               // proxy loads into processes since boot
    volatile LONG inits;                // Flc_Bluetooth_Init calls wrapped
    volatile LONG origOk;               // 1 = the original SDK loaded (0: every function answers "failed")
    BtState       st;
    volatile LONG cmdSeq;               // VestaHome: cmd, then cmdSeq + 1, then SetEvent(BT_CMD_EVENT)
    int           cmd;
    char          cmdText[40];          // BT_CMD_DIAL: the number
    volatile LONG cmdDone;              // the proxy: the cmdSeq it carried out
    int           cmdResult;            // what the SDK function returned (0 = ok)
    BtEvent       ev[BT_EV_SLOTS];
};

#endif
