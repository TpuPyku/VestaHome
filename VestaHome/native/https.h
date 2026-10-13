// HTTP/1.1 client with keep-alive, plain or TLS (BearSSL 0.6): the copy of OSMapsCE's native_osm/http.cpp,
// with free request headers and a lock (the weather and the camera download run in their own threads).
#ifndef VESTA_HTTPS_H
#define VESTA_HTTPS_H

enum { NET_OK, NET_DNS, NET_CONNECT, NET_TIMEOUT, NET_HTTP, NET_TLS, NET_MEMORY };

void HttpInit();
// Body of the answer (malloc'ed, zero-terminated) in *body; *status = the HTTP status. extraHeaders: lines
// ending with "\r\n", or NULL.
int  HttpGet(const char* host, bool https, const char* path, const char* extraHeaders, unsigned char** body,
             int* bodyLen, int* status);
void HttpSetGpsDate(int yyyymmdd);   // the unit's clock is reset on boot: certificates are checked by GPS date
void HttpCloseAll();
const wchar_t* NetErrorText(int err);

#endif
