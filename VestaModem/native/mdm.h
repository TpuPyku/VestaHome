// mdm.h - VestaModem's parts with no USB in them (tested on the PC: test_mdm.cpp): the APN of a SIM, the modem's
// status from its AT lines, the lines out of the byte stream, the byte queue between the USB reader and the COM port.
#ifndef MDM_H
#define MDM_H

struct MdmStat {
    int  rssi;       // 0..31, 99 not known (^RSSI, +CSQ); -1 not heard yet
    int  mode;       // the network (^MODE, ^SYSINFO sys_mode): 0 none, 3 GSM / GPRS, 5 WCDMA, ...; -1 not heard
    int  sim;        // ^SIMST, ^SYSINFO: 1 valid, 255 none, ...; -1 not heard
    char imsi[16];   // AT+CIMI's answer ("" - not asked yet)
};

void MdmStatInit(MdmStat* s);
bool MdmLine(MdmStat* s, const char* line);   // one line from the modem: true when the status changed
const char* MdmApn(const char* imsi);         // the operator's APN by its MCC + MNC; "internet" when not known

struct LineAcc {
    char buf[128];
    int  len;
};
typedef void (*LineFn)(void* ctx, const char* line);
void LineFeed(LineAcc* a, const BYTE* b, int n, LineFn fn, void* ctx);   // each non-empty line ended by CR or LF

struct Fifo {
    BYTE* b;
    int   cap, head, count;
};
int FifoPut(Fifo* f, const BYTE* b, int n);   // what fits of it; the bytes put
int FifoGet(Fifo* f, BYTE* b, int n);
int FifoFree(const Fifo* f);

#endif
