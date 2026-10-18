#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <enet/enet.h>
#include <stdlib.h>
#include "net.h"
#include "H264.hpp"
#include "Capture.hpp"
#include "shared.h"

// #include <miniupnpc/miniupnpc.h>
// #include <miniupnpc/upnpcommands.h> 
// #include <miniupnpc/upnperrors.h> 

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

    // int upnpError = 0;
    // // UPNPDev *devList = upnpDiscover(10, NULL, NULL, 0, 0, 2, &upnpError);
    // UPNPDev *devList = NULL;
    //
    // if (!devList) {
    //     printf("Server: Failed to find UPnP devices (Discovery error code: %d). Skipping automatic port forwarding.\n", upnpError);
    //     printf("Server: If hosting online, ensure port %u is manually forwarded.\n", port);
    // } else {
    //     UPNPUrls urls;
    //     IGDdatas data;
    //     char lanAddress[64];
    //     char wanAddress[64];
    //
    //     int status = UPNP_GetValidIGD(devList, &urls, &data, lanAddress, sizeof(lanAddress), wanAddress, sizeof(wanAddress));
    //     if (status != 1) {
    //         printf("Server: Found UPnP device, but no valid Internet Gateway IGD was found.\n");
    //     } else {
    //         printf("Server: Found router! Local IP: %s | Public IP: %s\n", lanAddress, wanAddress);
    //
    //         std::string portStr = std::to_string(port);
    //         int mapError = UPNP_AddPortMapping(
    //             urls.controlURL, 
    //             data.first.servicetype, 
    //             portStr.c_str(), // external port
    //             portStr.c_str(), // internal port
    //             lanAddress,      // computer's local IP
    //             "SSdemo",        // description for the router UI
    //             "UDP",           // protocol
    //             nullptr,         // remote host (null for any)
    //             "0"              // lease duration (infinite)
    //         );
    //
    //         if (mapError != UPNPCOMMAND_SUCCESS) {
    //             printf("Server: Failed to add UPnP port mapping: %s (code %d)\n", strupnperror(mapError), mapError);
    //         } else {
    //             printf("Server: Successfully port-forwarded UDP port %u via UPnP!\n", port);
    //         }
    //     }
    //     freeUPNPDevlist(devList);
    // }

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

                    for (size_t i = 0; i < server->host->peerCount; ++i) {
                        ENetPeer *currentPeer = &server->host->peers[i];
                        if (currentPeer->state == ENET_PEER_STATE_CONNECTED 
                                && currentPeer != event.peer) {
                                // ) {
                            ENetPacket *forwardPacket = enet_packet_create(
                                event.packet->data, 
                                event.packet->dataLength, 
                                event.packet->flags
                            );
                            enet_peer_send(currentPeer, VIDEO_BYTES, forwardPacket);
                        }
                    }
                    enet_host_flush(server->host);
                }
                enet_packet_destroy(event.packet);
            } 
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                printf("SERVER: Client disconnected.\n");
                
                u16 disconnected_id = event.peer->address.port;
                event.peer->data = NULL;

                u16 *data = (u16*) malloc(sizeof(u16));
                *data = disconnected_id;
                
                ENetPacket *packet = enet_packet_create(data, sizeof(u16), ENET_PACKET_FLAG_RELIABLE);
                enet_host_broadcast(server->host, DISCONNECTION, packet);
                free(data);
            }
        }
    }
}

void runClient(Client *client, volatile bool *running) {
    ENetEvent event;
    int tick = 0;

    ScreenCapturer* cap = init_screen_capture(0, 0);
    while (cap->width == 0 && *running) {
        SDL_Delay(10);
    }

    gstate->format = SDL_PIXELFORMAT_RGBA32;

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

                if (event.channelID == DISCONNECTION) {
                    u16 disconnected_peer_id = *((u16*) event.packet->data);
                    printf("CLIENT: Peer %u disconnected notification received.\n", disconnected_peer_id);

                    PeerSession* session = get_peer_session(disconnected_peer_id);
                    if (session) {
                        session->active = false; 
                        
                        if (session->pixels) {
                            delete[] session->pixels;
                        }

                        if (session->dec) {
                            free_h264_decoder(session->dec);
                        }
                        
                        printf("CLIENT: Successfully freed resources for peer %u\n", disconnected_peer_id);
                    } else {
                        printf("CLIENT: Warning - session not found for disconnected peer ID %u\n", disconnected_peer_id);
                    }
                }

                enet_packet_destroy(event.packet);
            } 
            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                puts("CLIENT: Disconnected from server.\n");
            }
        }

        if (tick++ % 1 == 0) {
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
    }
}
