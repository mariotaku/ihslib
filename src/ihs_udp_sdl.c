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

#include <SDL.h>
#include <SDL_net.h>

struct IHS_UDPSocket {
    UDPsocket socket;
    UDPpacket *packet;
};

static void AddressFromSDL(IHS_SocketAddress *ihs, const IPaddress *sdl);

static void AddressToSDL(const IHS_SocketAddress *ihs, IPaddress *sdl);

IHS_UDPSocket *IHS_UDPSocketOpen() {
    IHS_UDPSocket *socket = SDL_malloc(sizeof(IHS_UDPSocket));
    socket->packet = SDLNet_AllocPacket(2048);
    socket->socket = SDLNet_UDP_Open(0);
    return socket;
}

void IHS_UDPSocketClose(IHS_UDPSocket *socket) {
    SDLNet_UDP_Close(socket->socket);
    SDLNet_FreePacket(socket->packet);
    SDL_free(socket);
}

int IHS_UDPSocketReceive(IHS_UDPSocket *socket, IHS_UDPPacket *packet) {
    int ret;
    if ((ret = SDLNet_UDP_Recv(socket->socket, socket->packet)) <= 0) {
        return ret;
    }
    AddressFromSDL(&packet->address, &socket->packet->address);
    packet->buffer = socket->packet->data;
    packet->length = socket->packet->len;
    return ret;
}

int IHS_UDPSocketSend(IHS_UDPSocket *socket, IHS_UDPPacket *packet) {
    UDPpacket sdlPacket;
    SDL_memset(&sdlPacket, 0, sizeof(UDPpacket));
    AddressToSDL(&packet->address, &sdlPacket.address);
    sdlPacket.data = packet->buffer;
    sdlPacket.len = packet->length;
    return SDLNet_UDP_Send(socket->socket, -1, &sdlPacket);
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