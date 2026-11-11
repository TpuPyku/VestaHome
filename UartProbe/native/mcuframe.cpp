#include "mcuframe.h"

int McuFrame(int id, const unsigned char* data, int n, unsigned char* out)
{
    out[0] = (unsigned char)(n + 5);
    out[1] = 0xFF;
    out[2] = (unsigned char)(id >> 8);
    out[3] = (unsigned char)id;
    out[4] = 0xFF;
    unsigned char x = 0;
    for (int i = 0; i < n; i++)
        out[5 + i] = data[i];
    for (int i = 0; i < n + 5; i++)
        x ^= out[i];
    out[n + 5] = x;
    return n + 6;
}

int McuReply(const unsigned char* b, int n, int* id, int* status)
{
    for (int i = 0; i + 7 <= n; i++) {
        const unsigned char* r = b + i;
        unsigned char x = 0;
        for (int k = 0; k < 6; k++)
            x ^= r[k];
        if (r[0] == 6 && r[1] == 0xFF && r[4] == 0xFF && x == r[6]) {
            *id = r[2] << 8 | r[3];
            *status = r[5];
            return i;
        }
    }
    return -1;
}
