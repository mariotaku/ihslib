/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Ningyuan Li <https://github.com/mariotaku>.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "ihs_udp.h"
#include "ihs_buffer.h"

#include <SDL.h>
#include <SDL_net.h>

#include <stdlib.h>
#include <time.h>

struct IHS_UDPSocket {
    SDLNet_SocketSet sockets;
    UDPsocket socket;
    UDPsocket unblock;
    Uint16 interrupt_port;
    UDPpacket *packet;
};

static void AddressFromSDL(IHS_SocketAddress *ihs, const IPaddress *sdl);

static void AddressToSDL(const IHS_SocketAddress *ihs, IPaddress *sdl);

IHS_UDPSocket *IHS_UDPSocketOpen() {
    IHS_UDPSocket *socket = SDL_malloc(sizeof(IHS_UDPSocket));
    SDL_zerop(socket);
    socket->sockets = SDLNet_AllocSocketSet(2);
    socket->packet = SDLNet_AllocPacket(2048);
    socket->socket = SDLNet_UDP_Open(0);

    socket->interrupt_port = 53000;

    srand(time(NULL));

    while (socket->unblock == NULL) {
        socket->interrupt_port = 49152 + rand() / 262144;
        socket->unblock = SDLNet_UDP_Open(socket->interrupt_port);
    }
    SDL_assert_always(socket->unblock != NULL);

    SDLNet_UDP_AddSocket(socket->sockets, socket->socket);
    SDLNet_UDP_AddSocket(socket->sockets, socket->unblock);
    return socket;
}

void IHS_UDPSocketClose(IHS_UDPSocket *socket) {
    SDLNet_UDP_DelSocket(socket->sockets, socket->unblock);
    SDLNet_UDP_DelSocket(socket->sockets, socket->socket);

    SDLNet_UDP_Close(socket->unblock);
    SDLNet_UDP_Close(socket->socket);
    SDLNet_FreePacket(socket->packet);
    SDLNet_FreeSocketSet(socket->sockets);
    SDL_free(socket);
}

int IHS_UDPSocketReceive(IHS_UDPSocket *socket, IHS_UDPPacket *packet) {
    int ret;
    if ((ret = SDLNet_CheckSockets(socket->sockets, -1)) <= 0) {
        return ret;
    }
    if (SDLNet_SocketReady(socket->unblock)) {
        // Drain interrupt packets
        while (SDLNet_UDP_Recv(socket->unblock, socket->packet) > 0);
    }
    if (!SDLNet_SocketReady(socket->socket)) {
        return 0;
    }
    if ((ret = SDLNet_UDP_Recv(socket->socket, socket->packet)) <= 0) {
        return ret;
    }
    AddressFromSDL(&packet->address, &socket->packet->address);
    IHS_BufferAppendMem(&packet->buffer, socket->packet->data, socket->packet->len);
    return ret;
}

int IHS_UDPSocketSend(IHS_UDPSocket *socket, IHS_UDPPacket *packet) {
    UDPpacket sdlPacket;
    SDL_memset(&sdlPacket, 0, sizeof(UDPpacket));
    AddressToSDL(&packet->address, &sdlPacket.address);
    sdlPacket.data = IHS_BufferPointer(&packet->buffer);
    sdlPacket.len = (int) packet->buffer.size;
    return SDLNet_UDP_Send(socket->socket, -1, &sdlPacket);
}

int IHS_UDPSocketUnblock(IHS_UDPSocket *socket) {
    SDL_assert_always(socket != NULL);
    UDPpacket sdlPacket;
    SDL_memset(&sdlPacket, 0, sizeof(UDPpacket));
    sdlPacket.address.host = SDL_SwapBE32(INADDR_LOOPBACK);
    sdlPacket.address.port = SDL_SwapBE16(socket->interrupt_port);
    static Uint8 empty[1] = {0};
    sdlPacket.data = empty;
    sdlPacket.len = 1;
    return SDLNet_UDP_Send(socket->unblock, -1, &sdlPacket);
}

static void AddressFromSDL(IHS_SocketAddress *ihs, const IPaddress *sdl) {
    ihs->ip.v4.family = IHS_IPAddressFamilyIPv4;
    SDL_memcpy(&ihs->ip.v4.data, &sdl->host, 4);
    ihs->port = SDL_SwapBE16(sdl->port);
}

static void AddressToSDL(const IHS_SocketAddress *ihs, IPaddress *sdl) {
    SDL_memcpy(&sdl->host, ihs->ip.v4.data, 4);
    sdl->port = SDL_SwapBE16(ihs->port);
}