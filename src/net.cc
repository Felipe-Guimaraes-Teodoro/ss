#include <assert.h>
#include <raylib.h>
#include <stddef.h>
#include <stdio.h>
#include <enet/enet.h>
#include <stdlib.h>
#include "net.h"
#include "H264.h"
#include "Capture.h"
#include "shared.h"

PeerSession sessions[MAX_PEERS];

PeerSession* get_or_create_peer_session(u16 peer_id, int width, int height) {
    for (int i = 0; i < MAX_PEERS; ++i) {
        if (sessions[i].active && sessions[i].peerId == peer_id) {
            sessions[i].width = width;
            sessions[i].height = height;
            return &sessions[i];
        }
    }
    
    for (int i = 0; i < MAX_PEERS; ++i) {
        if (!sessions[i].active) {
            printf("Creating new session for peer ID %u (width: %d, height: %d)\n", peer_id, width, height);
            sessions[i].peerId = peer_id;
            sessions[i].width = width;
            sessions[i].height = height;
            sessions[i].dec = init_h264_decoder(sessions[i].width, sessions[i].height);
            sessions[i].pixels = NULL;
            sessions[i].active = true;
            return &sessions[i];
        }
    }
    
    fprintf(stderr, "Error: Max peers limit reached (%d), cannot create session for peer ID %u!\n", MAX_PEERS, peer_id);
    return NULL;
}

PeerSession* get_peer_session(u16 peer_id) {
    for (int i = 0; i < MAX_PEERS; ++i) {
        if (sessions[i].active && sessions[i].peerId == peer_id) {
            return &sessions[i];
        }
    }
    return NULL;
}

Server *createServer(u32 port) {
    Server *self = (Server*)malloc(sizeof(Server));
    if (!self) return NULL;

    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    self->addr = addr;
    self->host = enet_host_create(&addr, MAX_PEERS, N_CHANNELS, 0, 0);

    assert(self && self->host);
    return self;
}


Client *createClient(ENetAddress server_addr) {
    Client *self = (Client*)malloc(sizeof(Client));
    ENetEvent event;

    self->host = enet_host_create(NULL, 1, N_CHANNELS, 0, 0);
    assert(self->host);

    self->addr = server_addr; 

    self->peer = enet_host_connect(self->host, &self->addr, N_CHANNELS, 0);
    assert(self->peer);

    if (enet_host_service(self->host, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        puts("CLIENT: connection succeeded\n");
    } else {
        enet_peer_reset(self->peer);
        puts("CLIENT: connection failed\n");
    }

    return self;
}

void broadcastPacket(Server *server, ENetPeer *sender, bool broadcast_to_self, u8 channel_id, ENetPacket *packet) {
    broadcast_to_self = true;
    ENetPacket *packet_to_send = enet_packet_create(packet->data, packet->dataLength, packet->flags);
    
    for (size_t i = 0; i < server->host->peerCount; ++i) {
        ENetPeer *currentPeer = &server->host->peers[i];
        
        if (currentPeer->state != ENET_PEER_STATE_CONNECTED) continue;
        if (!broadcast_to_self && currentPeer == sender) continue;
        
        enet_peer_send(currentPeer, channel_id, packet_to_send);
    }
    
    if (packet_to_send->referenceCount == 0) {
        enet_packet_destroy(packet_to_send);
    }
}

void runServer(Server *server, volatile bool *running) {
    ENetEvent event;

    while (*running) {
        while (enet_host_service(server->host, &event, TICK_MS) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                printf("SERVER: A new client connected from %x:%u (port: %u).\n", 
                       event.peer->address.host, 
                       event.peer->address.port,
                       event.peer->address.port);
            } 
            else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                if (event.channelID == VIDEO_BYTES) {
                    u16 sender_port = event.peer->address.port;

                    if (event.packet->dataLength >= sizeof(u16)) {
                        u16 *packet_id_ptr = (u16*)event.packet->data;
                        *packet_id_ptr = sender_port; // injects the true sender port
                    }

                    broadcastPacket(server, event.peer, false, VIDEO_BYTES, event.packet);
                }
                else if (event.channelID == SHARING_STATE) {
                    u16 sender_port = event.peer->address.port;
                    if (event.packet->dataLength >= sizeof(SharingStatePacket)) {
                        SharingStatePacket *packet = (SharingStatePacket*) event.packet->data;
                        packet->peerId = sender_port; // injects the true sender port
                    }

                    broadcastPacket(server, event.peer, false, SHARING_STATE, event.packet);
                }
                
                enet_packet_destroy(event.packet);
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                printf("SERVER: Client disconnected.\n");
                
                u16 disconnected_id = event.peer->address.port;
                event.peer->data = NULL;

                u16 data = disconnected_id;
                ENetPacket *packet = enet_packet_create(&data, sizeof(u16), ENET_PACKET_FLAG_RELIABLE);
                
                enet_host_broadcast(server->host, DISCONNECTION, packet);
            }
        }
    }

    for (size_t i = 0; i < server->host->peerCount; ++i) {
        ENetPeer *peer = &server->host->peers[i];

        if (peer->state == ENET_PEER_STATE_CONNECTED) {
            enet_peer_disconnect(peer, NET_EXIT_SUCCESS);
        }
    }

    while (enet_host_service(server->host, &event, 100) > 0) {
        if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            event.peer->data = NULL;
        }
    }

    enet_host_destroy(server->host);
    gstate->server = NULL;
    free(server);
}

void runClient(Client *client, volatile bool *running) {
    ENetEvent event;
    int tick = 0;
    bool last_sharing_state = false;

    ScreenCapturer* cap = init_screen_capture(0, 0);
    while (cap->width == 0 && *running) {
        // yield momentarily if capture is initializing
    }

    H264Encoder *enc = init_h264_encoder(cap->width, cap->height);

    while (*running) {
        while (enet_host_service(client->host, &event, TICK_MS) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {

                if (event.channelID == VIDEO_BYTES) {
                    u8 *data = (u8*) event.packet->data;
                    
                    u16 sender_id = 0;
                    EncodedPacket pkt = deserialize_packet(data, &sender_id);
                    
                    PeerSession* session = get_or_create_peer_session(sender_id, pkt.width, pkt.height);

                    if (session && session->dec) {
                        session->pixels = decode_frame(session->dec, pkt.data, pkt.size);
                    }

                    free(pkt.data);
                }
                else if (event.channelID == DISCONNECTION) {
                    u16 disconnected_peer_id = *((u16*) event.packet->data);
                    printf("CLIENT: Peer %u disconnected notification received.\n", disconnected_peer_id);

                    PeerSession* session = get_peer_session(disconnected_peer_id);
                    if (session) {
                        session->active = false; 
                        
                        if (session->pixels) {
                            delete[] session->pixels;
                            session->pixels = NULL; // Fix dangling pointer
                        }

                        if (session->dec) {
                            free_h264_decoder(session->dec);
                            session->dec = NULL; // Fix dangling pointer
                        }
                        
                        printf("CLIENT: Successfully freed resources for peer %u\n", disconnected_peer_id);
                    } else {
                        printf("CLIENT: Warning - session not found for disconnected peer ID %u\n", disconnected_peer_id);
                    }
                }
                else if (event.channelID == SHARING_STATE) {
                    SharingStatePacket* state = (SharingStatePacket*) event.packet->data;
                    PeerSession* session = get_peer_session(state->peerId);
                    if (session) {
                        session->sharing = state->sharing; 
                        
                        if (session->pixels && !session->sharing) {
                            delete[] session->pixels;
                            session->pixels = NULL;
                        }
                    } else {
                        printf("CLIENT: Warning - session not found for \"stopped sharing\" peer ID %u\n", state->peerId);
                    }
                }

                enet_packet_destroy(event.packet);
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                puts("CLIENT: Disconnected from server.\n");
                if (event.data == NET_EXIT_SUCCESS) {
                    gstate->clientRunning = false; 
                } else {
                    // Fix hardcoded channel amount 
                    if (client->peer)
                        client->peer = enet_host_connect(client->host, &client->addr, N_CHANNELS, 0);
                    else  {
                        gstate->app_state = DISCONNECTING;
                        printf("CLIENT: failed to reconnect to peer\n");
                    }
                }
            }
        }

        if (tick++ % 1 == 0 && gstate->sharing) {
            u8* frame = capture_next_frame(cap);
            if (frame != NULL) {
                EncodedPacket pkt = encode_frame(enc, frame, cap->width, cap->height);
                size_t size = 0;
                 
                if (pkt.size > 0) {
                    u8* data = serialize_packet(&pkt, &size);
                    ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(client->peer, VIDEO_BYTES, packet);
                    enet_host_flush(client->host);
                    free(data);
                } 
            }
            release_captured_frame(cap);
        }

        // only send state update when sharing state actually changes
        if (gstate->sharing != last_sharing_state) {
            SharingStatePacket data;
            data.sharing = gstate->sharing;
            
            ENetPacket *packet = enet_packet_create(&data, sizeof(SharingStatePacket), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(client->peer, SHARING_STATE, packet);
            enet_host_flush(client->host);
            
            last_sharing_state = gstate->sharing;
        }
    }

    if (gstate->client && gstate->client->peer) {
        enet_peer_disconnect(gstate->client->peer, NET_EXIT_SUCCESS);
    }

    enet_host_destroy(client->host);
    free_h264_encoder(enc);
    free_screen_capture(cap);

    for (int i = 0; i < MAX_PEERS; ++i) {
        if (sessions[i].active) {
            if (sessions[i].pixels) {
                delete[] sessions[i].pixels;
                sessions[i].pixels = NULL;
            }
            if (sessions[i].dec) {
                free_h264_decoder(sessions[i].dec);
                sessions[i].dec = NULL;
            }
            sessions[i].active = false;
        }
    }

    gstate->client = NULL;
    free(client);
}

void *serverThreadFn(void* arg) {
    Server *server = (Server*) arg;
    runServer(server, &gstate->serverRunning);
    return NULL;
}

void *clientThreadFn(void *arg) {
    Client *client = (Client*) arg;
    runClient(client, &gstate->clientRunning);
    return NULL;
}

void initServer(GUIContext &gctx) {
    if (gctx.ping_input.text_length == 0) {
        gstate->server = createServer(PORT);
        pthread_t serverThread;
        gstate->serverRunning = true;
        pthread_create(&serverThread, NULL, serverThreadFn, gstate->server);
        pthread_detach(serverThread);
    } 
}

void initClient(const char* name) {
    ENetAddress addr;
    addr.port = PORT; 
    enet_address_set_host(&addr, name);
    
    gstate->client = createClient(addr);
    pthread_t clientThread;
    gstate->clientRunning = true; 
    pthread_create(&clientThread, NULL, clientThreadFn, gstate->client);
    pthread_detach(clientThread);

    gstate->app_state = CONNECTED;
}

