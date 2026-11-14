// numpad.cpp - see numpad.h
#include "numpad.h"
#include <string.h>

static bool Digit(wchar_t c)
{
    return c >= L'0' && c <= L'9';
}

static void DateFrom(wchar_t* text, const wchar_t* d, int n)   // the digits as ДД.ММ.ГГГГ, the dots where digits follow
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (i == 2 || i == 4)
            text[k++] = L'.';
        text[k++] = d[i];
    }
    text[k] = 0;
}

static int DateDigits(const wchar_t* text, wchar_t* d)   // up to 8
{
    int n = 0;
    for (; *text && n < 8; text++)
        if (Digit(*text))
            d[n++] = *text;
    return n;
}

static void Append(wchar_t* text, int cap, const wchar_t* add)
{
    size_t n = wcslen(text);
    if (n + wcslen(add) <= (size_t)cap)
        wcscpy(text + n, add);
}

static void DecType(wchar_t* text, int cap, wchar_t ch)
{
    size_t n = wcslen(text);
    if (ch == L'-') {
        if (text[0] == L'-')
            memmove(text, text + 1, n * sizeof(wchar_t));
        else if (n < (size_t)cap) {
            memmove(text + 1, text, (n + 1) * sizeof(wchar_t));
            text[0] = L'-';
        }
    } else if (ch == L'.') {
        if (!wcschr(text, L'.'))
            Append(text, cap, n == 0 || (n == 1 && text[0] == L'-') ? L"0." : L".");
    } else if (Digit(ch)) {
        wchar_t s[2] = { ch, 0 };
        Append(text, cap, s);
    }
}

void NumType(int kind, wchar_t* text, int cap, wchar_t ch)
{
    if (kind == NUM_DEC) {
        DecType(text, cap, ch);
        return;
    }
    if (!Digit(ch))
        return;
    if (kind == NUM_DATE) {
        wchar_t d[9];
        int n = DateDigits(text, d);
        if (n < 8)
            d[n++] = ch;
        DateFrom(text, d, n);
        return;
    }
    wchar_t s[2] = { ch, 0 };
    Append(text, cap, s);
}

void NumBack(int kind, wchar_t* text)
{
    if (kind == NUM_DATE) {
        wchar_t d[9];
        int n = DateDigits(text, d);
        DateFrom(text, d, n > 0 ? n - 1 : 0);
        return;
    }
    size_t n = wcslen(text);
    if (n)
        text[n - 1] = 0;
}

bool NumKeyShown(int kind, wchar_t ch)
{
    return kind == NUM_DEC || (ch != L'.' && ch != L'-');
}
