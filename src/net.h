#pragma once

#include <enet/enet.h>
#include "core.h"
#include "H264.h"
#include "gui.h"

#define PORT 7777

struct H264Decoder;
typedef struct {
    u32 peerId;
    H264Decoder *dec;
    int width, height;
    u8* pixels;
    bool active;
} PeerSession;

#define MAX_PEERS 32
#define TICK_MS 16 

extern PeerSession sessions[MAX_PEERS];

typedef struct {
    u32 disconectedPeerId;
} DisconnectionPacket;

typedef enum {
    NET_EXIT_SUCCESS,
    NUM_NET_EXIT_CODES,
} ExitCode;

typedef enum {
    VIDEO_BYTES,
    DISCONNECTION,
    OTHER,
    N_CHANNELS,
} Channel;

typedef struct {
    ENetAddress addr;
    ENetHost *host;
} Server; 

typedef struct {
    ENetAddress addr;
    ENetHost *host;
    ENetPeer *peer;
} Client; 

Server *createServer(u32 port);
Client *createClient(ENetAddress addr); 

// Dispatches the run functions on different threads 
// and connects to the appropriate address

void initServer(GUIContext &gctx);
void initClient(const char* host_name); 

