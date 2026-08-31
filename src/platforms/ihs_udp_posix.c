/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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
#include "ihs_thread.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include <assert.h>

struct IHS_UDPSocket {
    int fd;
    /**
     * Self-pipe, so a blocked receive can be woken without waiting for a datagram. Portable across
     * POSIX, unlike eventfd.
     */
    int wakeup[2];
    IHS_Mutex *mutex;
};

static void AddressFromSys(IHS_SocketAddress *ihs, const struct sockaddr_storage *sys);

static size_t AddressToSys(const IHS_SocketAddress *ihs, struct sockaddr_storage *sys);

IHS_UDPSocket *IHS_UDPSocketOpen(bool broadcast) {
    IHS_UDPSocket *s = calloc(1, sizeof(IHS_UDPSocket));
    s->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(s->fd >= 0);
    if (pipe(s->wakeup) != 0) {
        s->wakeup[0] = -1;
        s->wakeup[1] = -1;
    } else {
        // Non-blocking both ways: a wake must never block the caller, and draining must never block
        // the receiver when another thread got there first.
        fcntl(s->wakeup[0], F_SETFL, O_NONBLOCK);
        fcntl(s->wakeup[1], F_SETFL, O_NONBLOCK);
    }
    s->mutex = IHS_MutexCreate();
    assert(s->mutex != NULL);
    if (broadcast) {
        uint32_t opt = 1;
        setsockopt(s->fd, SOL_SOCKET, SO_BROADCAST, (char *) &opt, sizeof(opt));
    }
    return s;
}

void IHS_UDPSocketClose(IHS_UDPSocket *s) {
    if (s->wakeup[0] >= 0) {
        close(s->wakeup[0]);
        close(s->wakeup[1]);
        s->wakeup[0] = -1;
        s->wakeup[1] = -1;
    }
    IHS_MutexDestroy(s->mutex);
    close(s->fd);
    free(s);
}

int IHS_UDPSocketReceive(IHS_UDPSocket *s, IHS_UDPPacket *packet, int timeoutMs) {
    struct pollfd fds[2];
    nfds_t nfds = 1;
    fds[0].fd = s->fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    if (s->wakeup[0] >= 0) {
        fds[1].fd = s->wakeup[0];
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        nfds = 2;
    }
    int ready = poll(fds, nfds, timeoutMs);
    if (ready < 0) {
        // A signal is not an error to the caller; it just means nothing arrived yet.
        return errno == EINTR ? 0 : -1;
    }
    if (ready == 0) {
        return 0;
    }
    if (nfds > 1 && (fds[1].revents & POLLIN)) {
        // Drain every pending wake; several may have been coalesced.
        uint8_t discard[64];
        while (read(s->wakeup[0], discard, sizeof(discard)) > 0);
    }
    if (!(fds[0].revents & POLLIN)) {
        return 0;
    }

    struct sockaddr_storage sender;
    socklen_t senderlen = sizeof(sender);
    ssize_t len;
    IHS_BufferEnsureMaxSize(&packet->buffer, 2048);
    if ((len = recvfrom(s->fd, IHS_BufferPointer(&packet->buffer), IHS_BufferMaxSize(&packet->buffer),
                        0, (struct sockaddr *) &sender, &senderlen)) <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            return 0;
        }
        return -1;
    }
    packet->buffer.size = len;
    AddressFromSys(&packet->address, &sender);
    return 1;
}

bool IHS_UDPSocketUnblock(IHS_UDPSocket *s) {
    if (s->wakeup[1] < 0) {
        return false;
    }
    const uint8_t one = 1;
    // EAGAIN means the pipe is already full of pending wakes, which is just as good.
    ssize_t written = write(s->wakeup[1], &one, 1);
    return written == 1 || errno == EAGAIN || errno == EWOULDBLOCK;
}

bool IHS_UDPSocketSend(IHS_UDPSocket *s, const IHS_UDPPacket *packet) {
    struct sockaddr_storage addr;
    size_t addr_len = AddressToSys(&packet->address, &addr);
    IHS_MutexLock(s->mutex);
    bool ret = sendto(s->fd, IHS_BufferPointer(&packet->buffer), packet->buffer.size, 0,
                      (struct sockaddr *) &addr, addr_len) > 0;
    IHS_MutexUnlock(s->mutex);
    return ret;
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