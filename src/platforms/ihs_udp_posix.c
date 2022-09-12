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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <assert.h>

struct IHS_UDPSocket {
    int fd;
    int pipe[2];
};

static void AddressFromSys(IHS_SocketAddress *ihs, const struct sockaddr_storage *sys);

static size_t AddressToSys(const IHS_SocketAddress *ihs, struct sockaddr_storage *sys);

IHS_UDPSocket *IHS_UDPSocketOpen() {
    IHS_UDPSocket *s = calloc(1, sizeof(IHS_UDPSocket));

    s->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(s->fd >= 0);
    uint32_t broadcast = 1;
    setsockopt(s->fd, SOL_SOCKET, SO_BROADCAST, (char *) &broadcast, sizeof(broadcast));

    assert(pipe(s->pipe) == 0);
    fcntl(s->pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(s->pipe[1], F_SETFL, O_NONBLOCK);
    return s;
}

void IHS_UDPSocketClose(IHS_UDPSocket *s) {
    close(s->pipe[1]);
    close(s->pipe[0]);
    close(s->fd);
    free(s);
}

int IHS_UDPSocketReceive(IHS_UDPSocket *s, IHS_UDPPacket *packet) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s->fd, &fds);
    FD_SET(s->pipe[0], &fds);

    if (select((s->fd > s->pipe[0] ? s->fd : s->pipe[0]) + 1, &fds, NULL, NULL, NULL) <= 0) {
        return 0;
    }

    if (FD_ISSET(s->pipe[0], &fds)) {
        // Drain self pipe
        char buf[32];
        while (read(s->pipe[0], buf, 32) > 0);
    }

    if (!FD_ISSET(s->fd, &fds)) {
        return 0;
    }

    struct sockaddr_storage sender;
    socklen_t senderlen = sizeof(sender);
    ssize_t len;
    IHS_BufferEnsureMaxSize(&packet->buffer, 2048);
    if ((len = recvfrom(s->fd, IHS_BufferPointer(&packet->buffer), IHS_BufferMaxSize(&packet->buffer),
                        0, (struct sockaddr *) &sender, &senderlen)) <= 0) {
        return -1;
    }
    packet->buffer.size = len;
    AddressFromSys(&packet->address, &sender);
    return 1;
}

int IHS_UDPSocketSend(IHS_UDPSocket *s, IHS_UDPPacket *packet) {
    struct sockaddr_storage addr;
    size_t addr_len = AddressToSys(&packet->address, &addr);
    return sendto(s->fd, IHS_BufferPointer(&packet->buffer), packet->buffer.size, 0,
                  (struct sockaddr *) &addr, addr_len) > 0;
}

int IHS_UDPSocketUnblock(IHS_UDPSocket *s) {
    assert(s != NULL);
    const static uint8_t empty[1] = {0};
    return write(s->pipe[1], empty, 1) == 1;
}

static void AddressFromSys(IHS_SocketAddress *ihs, const struct sockaddr_storage *sys) {
    switch (sys->ss_family) {
        case AF_INET: {
            ihs->ip.family = IHS_IPAddressFamilyIPv4;
            const struct sockaddr_in *addr = (const struct sockaddr_in *) sys;
            memcpy(&ihs->ip.v4.data, &addr->sin_addr, 4);
            ihs->port = ntohs(addr->sin_port);
            break;
        }
        case AF_INET6: {
            ihs->ip.family = IHS_IPAddressFamilyIPv6;
            const struct sockaddr_in6 *addr = (const struct sockaddr_in6 *) sys;
            memcpy(&ihs->ip.v6.data, &addr->sin6_addr, 16);
            ihs->port = ntohs(addr->sin6_port);
            break;
        }
        default: {
            assert(sys->ss_family == AF_INET || sys->ss_family == AF_INET6);
            break;
        }
    }
}

static size_t AddressToSys(const IHS_SocketAddress *ihs, struct sockaddr_storage *sys) {
    switch (ihs->ip.family) {
        case IHS_IPAddressFamilyIPv4: {
            struct sockaddr_in *addr = (struct sockaddr_in *) sys;
            memset(addr, 0, sizeof(struct sockaddr_in));
            addr->sin_family = AF_INET;
            addr->sin_port = htons(ihs->port);
            memcpy(&addr->sin_addr, ihs->ip.v4.data, 4);
            return sizeof(*addr);
        }
        case IHS_IPAddressFamilyIPv6: {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *) sys;
            memset(addr, 0, sizeof(struct sockaddr_in6));
            addr->sin6_family = AF_INET6;
            addr->sin6_port = htons(ihs->port);
            memcpy(&addr->sin6_addr, ihs->ip.v6.data, 16);
            return sizeof(*addr);
        }
        default: {
            assert(ihs->ip.family == IHS_IPAddressFamilyIPv4 || ihs->ip.family == IHS_IPAddressFamilyIPv6);
            return -1;
        }
    }
}