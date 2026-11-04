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
//   0x38 PBAP: ev[1] 2 connected, 0 not   0x39 a pull's entries start   0x3A one entry (BtPb)   0x3B they end
//   0x45 PAN (the phone's internet, Bluetooth tethering): connected   0x4B its connect failed   0x46 disconnected
//        (FLC_BLUETOOTH_SDK's PanCallback, Ghidra 2026-10-31: XCBPAN user events 1 ok / 1 failed / 2)
// State: written under a sequence lock (seq odd while writing); a reader copies it between two equal even seqs.
#ifndef BTSHM_H
#define BTSHM_H

#define BT_SHM_NAME   L"VestaBT5"       // v5 (2026-10-31): PAN (v4: 16 paired, search / pair / unpair)
#define BT_CMD_EVENT  L"VestaBT5_Cmd"   // set by VestaHome after writing a command
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
       BT_CMD_UNPAIR /* cmdText: the address - Conn_Delete_Paired(addr), then the list again */,
       BT_CMD_SPPADDR /* cmdText: the address - Flc_Bluetooth_Spp_SetAddr(addr) (the serial port's device) */,
       BT_CMD_PBAP_PULL /* cmdText: "<storage> <list>" (0 the phone / 1 SIM, BT_PB_*): a list of the connected phone
                           into BtPb - PbapClient_Connect first if PBAP is not connected; cmdResult -2 = another
                           pull runs (ask again later) */,
       BT_CMD_SETOPT /* cmdText: "c 1" / "a 0" - the stack's connect at power-on (Conn_EnablePowerOnConnect) / the
                        auto answer after 5 s (Conn_EnableAutoAnswerIncomingCall, as the shell's settings) */,
       BT_CMD_GETOPTS /* cmdResult: bit 0 connect at power-on, bit 1 auto answer (Conn_Get*Status) */,
       BT_CMD_VISIBLE /* cmdText "1" / "0": the unit seen by phones (Conn_Enter_Pairing: the stack's scan mode 3) or
                         only connectable (Conn_Exit_Pairing: 2) - Ghidra 2026-10-23 */,
       BT_CMD_PAIRCONFIRM /* cmdText "1" / "0": a phone's pairing request (BtPair) accepted / refused
                             (Conn_Pair_Confirm) */,
       BT_CMD_SDP /* cmdText: the address - Conn_Search_SC_IAP(addr): the SDP search of SPP (0x1101) alone; the
                     answer: event 0x0B (the services found), 0x4F (one SPP) - OBD 2026-10-29: the stack's own
                     search of three UUIDs at once is where the clone's link failed */,
       BT_CMD_PAN /* cmdText: the phone's address - its internet over Bluetooth (the user 2026-10-31; Android's
                     "Bluetooth tethering"): Flc_Bluetooth_PanServer_Connect(addr, 0x1116 - the phone a NAP), the
                     answer an event (0x45 / 0x4B); "" - PanServer_Disconnect() */,
       BT_CMD_PHONE /* cmdText "0": Bluetooth for the OBD adapter alone (the user 2026-11-03: one device at a time;
                       the stack keeps one) - the stack's own connects (power-on, link lost) off, the phone's
                       internet, music and handsfree let go (PanServer_Disconnect, Av_Disconnect, Hf_Disconnect(0));
                       "1": the stack's connects on */,
       BT_CMD_POWER /* cmdText "0" / "1": Bluetooth off / on (the user 2026-11-04: a switch as Wi-Fi's) as the
                       stock shell's switch (CSetUpBTMenuDlg::SlotSwitchBT, Ghidra 2026-11-04): Flc_Bluetooth_Deinit
                       (it ends in a thread of its own: event 0x02) / Flc_Bluetooth_Init + EnablePairIndForApp(1)
                       (event 0x01); never PowerOff - the shell does not, it unloads the stack's drivers. While off
                       every export answers "failed" (1) and every other command -3 (the stack's locks are gone);
                       the state in BtShm::power */ };
enum { BT_POW_ON, BT_POW_GOING_OFF, BT_POW_OFF, BT_POW_GOING_ON };   // (0: a proxy older than it - on)
enum { BT_PAN_IDLE, BT_PAN_CONNECTING, BT_PAN_ONLINE, BT_PAN_FAILED };
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
    int   pan;                          // BT_PAN_*: the phone's internet (BT_CMD_PAN, events 0x45 / 0x4B / 0x46)
    DWORD panTick;
};

struct BtShm {
    DWORD         magic;
    DWORD         version;              // 5
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
    volatile LONG power;                // BT_POW_* (BT_CMD_POWER; at the end: older readers keep their offsets)
};

// The phone book and the call lists (PBAP, Ghidra 2026-10-21, PROJECT.md): every pull - the shell's own or
// VestaHome's (BT_CMD_PBAP_PULL) - lands here, its list told by the PullPhonebook call (intercepted); an entry is
// the SDK's event 0x3A, already parsed. VestaHome's pulls are hidden from the shell (its book stays its own).
// VestaHome copies the entries out once state is BT_PBS_DONE (seq: +1 at each pull's end).
#define BT_PB_NAME  L"VestaBTPB1"
#define BT_PB_MAX   1000
#define BT_PB_NUMS  3
enum { BT_PB_OCH, BT_PB_MCH, BT_PB_PB, BT_PB_CCH, BT_PB_ICH };   // dialled, missed, the book, all calls, received
enum { BT_PBS_IDLE, BT_PBS_CONNECTING, BT_PBS_PULLING, BT_PBS_DONE, BT_PBS_FAILED };
enum { BT_PBC_MISSED, BT_PBC_RECEIVED, BT_PBC_DIALLED };          // ev[0x4E] of a call (NCL_BT's mapping)

struct BtPbEntry {
    char          name[64];                   // UTF-8 (ev + 4)
    char          num[BT_PB_NUMS][24];        // the first numbers (the SDK's six lists ev[0x41..0x46], in order)
    unsigned char numKind[BT_PB_NUMS];        // which of the six (the TEL types; which is which - not known yet)
    unsigned char callType;                   // BT_PBC_* (the call lists)
    char          time[20];                   // the call's "20261021T153000" (ev + 0x11C)
};

struct BtPb {
    DWORD         magic;
    volatile LONG seq;                        // +1 when a pull ends (done or failed)
    int           storage, list;              // the pull running / the last one
    int           state;                      // BT_PBS_*
    int           count, total;               // entries kept / seen
    int           pbap;                       // 0x38 ev[1]: 2 connected, 0 not, -1 none yet
    DWORD         tick;                       // the state's last change
    BtPbEntry     e[BT_PB_MAX];
};

// A phone's pairing request (the SDK's event 4, NCL_BT's BT_CONN_PAIR_IND: the address at +4, the name at +0x16,
// the code to compare at +0x116; event 5 = the pairing's end, its result at +0x116): beyond the 64 bytes the events'
// ring keeps, so the proxy puts it here. The stock shell answers it with a window of its own; without the shell
// VestaHome does (BT_CMD_PAIRCONFIRM).
#define BT_PAIR_NAME L"VestaBTPair1"
struct BtPair {
    DWORD         magic;
    volatile LONG seq;                        // +1 at each request (seq even/odd not used: one writer, short)
    DWORD         tick;
    char          addr[18], name[64], code[16];
    volatile LONG doneSeq;                    // +1 at each pairing's end (event 5)
    int           result;                     // its result (non-zero = paired)
};

#endif
