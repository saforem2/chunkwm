#ifndef CHUNKWM_COMMON_DAEMON_H
#define CHUNKWM_COMMON_DAEMON_H

#define DAEMON_CALLBACK(name) void name(const char *Message, int SockFD)
typedef DAEMON_CALLBACK(daemon_callback);

bool StartDaemon(int Port, daemon_callback Callback);
bool StartDaemon(char *SocketPath, daemon_callback *Callback);

void StopDaemon();

bool ConnectToDaemon(int *SockFD, int Port);
bool ConnectToDaemon(int *SockFD, char *SocketPath);

void WriteToSocket(const char *Message, int SockFD);
char *ReadFromSocket(int SockFD);
void CloseSocket(int SockFD);

#endif
