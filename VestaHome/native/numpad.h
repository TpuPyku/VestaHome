// numpad.h - the digits' keyboard (the user 2026-11-14: the service's date and the home point typed as digits, not
// text): whole numbers (km), coordinates (a dot, the sign) and a date (ДД.ММ.ГГГГ, the dots put by itself). The text
// is edited at its end.
#pragma once
#include <wchar.h>

enum { NUM_TEXT, NUM_INT, NUM_DEC, NUM_DATE };

void NumType(int kind, wchar_t* text, int cap, wchar_t ch);   // a key into text (cap: characters at most)
void NumBack(int kind, wchar_t* text);                        // the last character (a date's: the last digit)
bool NumKeyShown(int kind, wchar_t ch);                       // the dot and minus keys: coordinates only
