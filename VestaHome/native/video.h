// Video through the stock shell's GStreamer 0.10 (\ResidentFlash\navigation\app, the CSR SDK its own player
// uses): playbin2 with ddrawsink in our window and waveoutsink, loaded at run time (the exe still imports
// COREDLL + WS2 only). The PC build has no GStreamer: VideoLoad fails there.
#ifndef VESTA_VIDEO_H
#define VESTA_VIDEO_H

enum { VID_NONE, VID_PLAYING, VID_PAUSED, VID_ENDED, VID_ERROR };

typedef void (*VideoLogFn)(const wchar_t* fmt, ...);

bool VideoLoad(VideoLogFn log, wchar_t* why, int whyCap);   // once; false with the reason
// Plays the file into `wnd` (its whole client area); stretch = fill the window, otherwise the picture keeps
// its proportions.
bool VideoOpen(const wchar_t* path, HWND wnd, bool stretch, wchar_t* why, int whyCap);
void VideoPause(bool pause);
void VideoSetStretch(bool stretch);
void VideoStop();
void VideoSeek(double sec);
bool VideoTimes(double* pos, double* len);   // seconds; false = nothing open
void VideoExpose();                          // the window moved or changed size
// Every 250 ms while a video is open: the bus messages. Returns the state; on VID_ERROR, why = the text.
int  VideoPoll(wchar_t* why, int whyCap);

#endif
