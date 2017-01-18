#ifndef CHUNKWM_COMMON_CLIENT_H
#define CHUNKWM_COMMON_CLIENT_H

bool ConnectToDaemon(int *SockFD, int Port);
void CloseSocket(int SockFD);

void WriteToSocket(const char *Message, int SockFD);
char *ReadFromSocket(int SockFD);

#endif
