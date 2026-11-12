// netmirror.h - the internet radio catalogue's mirror (the user 2026-11-12: radio-browser.info blocked for many; the
// stations play from their own addresses): VestaCore's weekly file (tools/make_radio_mirror.py, the release "radio")
// - radio-browser's answers to the paths VestaHome asks. Its form: "VHRM1\n", then for each answer its path, "\n",
// its length (decimal), "\n", the JSON.
#ifndef NETMIRROR_H
#define NETMIRROR_H

// The answer to path in the file's bytes (body points into data, blen its length); false: not there, or not a mirror.
bool NcMirrorFind(const unsigned char* data, int len, const char* path, const unsigned char** body, int* blen);

#endif
