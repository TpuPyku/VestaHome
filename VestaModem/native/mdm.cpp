// mdm.cpp - see mdm.h.
#include <windows.h>
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

const char* MdmApn(const char* imsi)
{
    static const char* kApn[][2] = { { "25001", "internet.mts.ru" }, { "25002", "internet" },
                                     { "25099", "internet.beeline.ru" }, { "25020", "internet.tele2.ru" },
                                     { "25011", "internet.yota" }, { "25062", "m.tinkoff" },
                                     { "25035", "inet.ycc.ru" }, { "40101", "internet.beeline.kz" },
                                     { "40177", "internet" }, { "25701", "internet.velcom.by" },
                                     { "25702", "mts" } };
    for (int i = 0; i < (int)(sizeof(kApn) / sizeof(kApn[0])); i++)
        if (!strncmp(imsi, kApn[i][0], 5))
            return kApn[i][1];
    return "internet";
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
