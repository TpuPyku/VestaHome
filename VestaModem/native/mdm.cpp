// mdm.cpp - see mdm.h.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdm.h"

void MdmStatInit(MdmStat* s)
{
    s->rssi = s->mode = s->sim = -1;
    s->imsi[0] = 0;
}

static bool Set(int* v, int to)
{
    if (*v == to)
        return false;
    *v = to;
    return true;
}

static const char* Field(const char* s, int k)   // the k-th of the comma separated fields (0 - the first)
{
    for (; k > 0 && s; k--) {
        s = strchr(s, ',');
        if (s)
            s++;
    }
    return s;
}

static bool Imsi(MdmStat* s, const char* line)
{
    int n = 0;
    while (line[n] >= '0' && line[n] <= '9')
        n++;
    if (line[n] || n != 15 || !strcmp(s->imsi, line))
        return false;
    strcpy(s->imsi, line);
    return true;
}

bool MdmLine(MdmStat* s, const char* line)
{
    if (!strncmp(line, "^RSSI:", 6))
        return Set(&s->rssi, atoi(line + 6));
    if (!strncmp(line, "+CSQ:", 5))
        return Set(&s->rssi, atoi(line + 5));
    if (!strncmp(line, "^MODE:", 6))
        return Set(&s->mode, atoi(line + 6));
    if (!strncmp(line, "^SIMST:", 7))
        return Set(&s->sim, atoi(line + 7));
    if (!strncmp(line, "^SYSINFO:", 9)) {   // srv_status, srv_domain, roam_status, sys_mode, sim_state, ...
        const char* mode = Field(line + 9, 3), *sim = Field(line + 9, 4);
        bool a = mode && Set(&s->mode, atoi(mode));
        bool b = sim && Set(&s->sim, atoi(sim));
        return a || b;
    }
    return Imsi(s, line);
}

// The operators (the user 2026-10-30: Russia's and some of the CIS's; Tele2 of Kazakhstan and T2 of Russia are two)
static const MdmOp kOps[] = {
    { "25001", L"МТС (Россия)", "internet.mts.ru", "mts", "mts" },
    { "25002", L"МегаФон", "internet", "gdata", "gdata" },
    { "25099", L"Билайн (Россия)", "internet.beeline.ru", "beeline", "beeline" },
    { "25020", L"T2 (Россия)", "internet.tele2.ru", "", "" },
    { "25011", L"Yota", "internet.yota", "", "" },
    { "25062", L"Т-Мобайл", "m.tinkoff", "", "" },
    { "25035", L"Мотив", "inet.ycc.ru", "motiv", "motiv" },
    { "40101", L"Beeline (Казахстан)", "internet.beeline.kz", "@internet.beeline", "beeline" },
    { "40102", L"Kcell", "internet", "", "" },
    { "40177", L"Tele2 (Казахстан)", "internet", "", "" },
    { "40107", L"Altel", "internet", "", "" },
    { "25701", L"A1 (Беларусь)", "internet.velcom.by", "", "" },
    { "25702", L"МТС (Беларусь)", "mts", "mts", "mts" },
    { "25704", L"life:) (Беларусь)", "internet.life.com.by", "", "" },
    { "43404", L"Beeline (Узбекистан)", "internet.beeline.uz", "beeline", "beeline" },
    { "43405", L"Ucell", "internet", "", "" },
    { "43701", L"Beeline (Киргизия)", "internet.beeline.kg", "", "" },
    { "43709", L"O! (Киргизия)", "internet", "", "" },
    { "28301", L"Beeline (Армения)", "internet.beeline.am", "", "" },
    { "40001", L"Azercell", "internet", "", "" },
};

const MdmOp* MdmOps(int* n)
{
    *n = (int)(sizeof(kOps) / sizeof(kOps[0]));
    return kOps;
}

const MdmOp* MdmOpBySim(const char* imsi)
{
    for (int i = 0; i < (int)(sizeof(kOps) / sizeof(kOps[0])); i++)
        if (!strncmp(imsi, kOps[i].sim, 5))
            return &kOps[i];
    return NULL;
}

const char* MdmApn(const char* imsi)
{
    const MdmOp* op = MdmOpBySim(imsi);
    return op ? op->apn : "internet";
}

bool MdmApnCommand(const char* apn, char* out, int cap)
{
    if (!apn[0] || strchr(apn, '"'))
        return false;
    _snprintf(out, cap, "AT+CGDCONT=1,\"IP\",\"%s\"\r", apn);
    out[cap - 1] = 0;
    return true;
}

void LineFeed(LineAcc* a, const BYTE* b, int n, LineFn fn, void* ctx)
{
    for (int i = 0; i < n; i++) {
        if (b[i] != '\r' && b[i] != '\n') {
            if (a->len < (int)sizeof(a->buf) - 1)   // (a longer one is cut)
                a->buf[a->len++] = (char)b[i];
            continue;
        }
        if (!a->len)
            continue;
        a->buf[a->len] = 0;
        a->len = 0;
        fn(ctx, a->buf);
    }
}

int FifoFree(const Fifo* f)
{
    return f->cap - f->count;
}

int FifoPut(Fifo* f, const BYTE* b, int n)
{
    if (n > FifoFree(f))
        n = FifoFree(f);
    for (int i = 0; i < n; i++)
        f->b[(f->head + f->count + i) % f->cap] = b[i];
    f->count += n;
    return n;
}

int FifoGet(Fifo* f, BYTE* b, int n)
{
    if (n > f->count)
        n = f->count;
    for (int i = 0; i < n; i++)
        b[i] = f->b[(f->head + i) % f->cap];
    f->head = (f->head + n) % f->cap;
    f->count -= n;
    return n;
}
