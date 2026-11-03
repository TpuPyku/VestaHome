// test_btapi.cpp - a stand-in btapi.dll for test_btrf (BtRfLoad): btapi's names, each call answering its own number.
#define X extern "C" __declspec(dllexport) int
X BT_Startup() { return 101; }
X BT_Cleanup() { return 102; }
X BTDDB_AddDevice(const unsigned char*, int, int*) { return 103; }
X BTDDB_BindToACL(int) { return 104; }
X BTDDB_UnbindFromACL(int) { return 105; }
X XCBM_GetConnections(unsigned char*, int*) { return 106; }
X BTRF_RegisterUser(void*, int*) { return 107; }
X BTRF_DeregisterUser(int) { return 108; }
X BTRF_OpenClientChannelEx(int, int, int, int, int, int, int*, int, int) { return 109; }
X BTRF_SendData(int, const void*, int, int) { return 110; }
X BTRF_CloseChannel(int) { return 111; }
X BTRF_AdvanceCredit(int, int) { return 112; }
