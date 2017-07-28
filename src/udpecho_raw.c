/*
 * Copyright (c) 2016 Stephan Linz <linz@li-pro.net>, Li-Pro.Net
 * All rights reserved.
 *
 * Based on examples provided by
 * Iwan Budi Kusnanto <ibk@labhijau.net> (https://gist.github.com/iwanbk/1399729)
 * Juri Haberland <juri@sapienti-sat.org> (https://lists.gnu.org/archive/html/lwip-users/2007-06/msg00078.html)
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of and a contribution to the lwIP TCP/IP stack.
 *
 * Credits go to Adam Dunkels (and the current maintainers) of this software.
 *
 * Stephan Linz rewrote this file to get a basic echo example.
 */

/**
 * @file
 * UDP echo server example using raw API.
 *
 * Echos all bytes sent by connecting client,
 * and passively closes when client is done.
 *
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <error.h>
#include <time.h>
#include "lwip/opt.h"
#include "lwip/udp.h"

#include "udpecho_raw.h"

#if LWIP_UDP

static struct udp_pcb *udpecho_raw_pcb;

typedef struct {
    char *buffer;
    int length;
} response;

int SOCKS_PORT = 1080;
char *SOCKS_ADDR = {"127.0.0.1"};

void tcp_dns_query(void *query, response *buffer, int len) {
  int sock;
  struct sockaddr_in socks_server;
  char tmp[1024];

  memset(&socks_server, 0, sizeof(socks_server));
  socks_server.sin_family = AF_INET;
  socks_server.sin_port = htons(SOCKS_PORT);
  socks_server.sin_addr.s_addr = inet_addr(SOCKS_ADDR);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    printf("[!] Error creating TCP socket");

  if (connect(sock, (struct sockaddr *) &socks_server, sizeof(socks_server)) < 0)
    printf("[!] Error connecting to proxy");

  // socks handshake
  send(sock, "\x05\x01\x00", 3, 0);
  recv(sock, tmp, 1024, 0);

  srand(time(NULL));

  // select random dns server
  in_addr_t remote_dns = inet_addr("8.8.8.8");
  memcpy(tmp, "\x05\x01\x00\x01", 4);
  memcpy(tmp + 4, &remote_dns, 4);
  memcpy(tmp + 8, "\x00\x35", 2);

  printf("Using DNS server: %s\n", inet_ntoa(*(struct in_addr *) &remote_dns));

  send(sock, tmp, 10, 0);
  recv(sock, tmp, 1024, 0);

  // forward dns query
  send(sock, query, len, 0);
  buffer->length = recv(sock, buffer->buffer, 2048, 0);
}

/**
 * receive callback for a UDP PCB
 * pcb->recv(pcb->recv_arg, pcb, p, ip_current_src_addr(), src_port)
 */
static void
udpecho_raw_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 const ip_addr_t *addr, u16_t port) {
  LWIP_UNUSED_ARG(arg);
  if (p != NULL) {
    char localip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(upcb->local_ip), localip_str, INET_ADDRSTRLEN);
    char remoteip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(upcb->remote_ip), remoteip_str, INET_ADDRSTRLEN);

    // flow 119.23.211.95:80 <-> 172.16.0.1:53536
    printf("<======== udp flow %s:%d <-> %s:%d\n", localip_str, upcb->local_port, remoteip_str, upcb->remote_port);
    inet_ntop(AF_INET, &addr, localip_str, INET_ADDRSTRLEN);
    printf("<======== addr:port %s:%d\n", localip_str, port);


    response *buffer = (response *) malloc(sizeof(response));
    buffer->buffer = malloc(2048);
    char *query;

    pbuf_copy_partial(p, buffer->buffer, p->tot_len, 0);

    query = malloc(p->len + 3);
    query[0] = 0;
    query[1] = (char) p->len;
    memcpy(query + 2, buffer->buffer, p->len);

    // forward the packet to the tcp dns server
    tcp_dns_query(query, buffer, p->len + 2);

    if (buffer->length > 0) {
      /* send received packet back to sender */
      struct pbuf *socksp = pbuf_alloc(PBUF_TRANSPORT, (uint16_t) buffer->length - 2, PBUF_RAM);
      memcpy(socksp->payload, buffer->buffer + 2, (size_t) buffer->length - 2);
      udp_sendto(upcb, socksp, addr, port);
      /* free the pbuf */
      pbuf_free(socksp);
    }

    free(buffer->buffer);
    free(buffer);
    free(query);
    pbuf_free(p);
  }
}

void
udpecho_raw_init(void) {
  /* call udp_new */
  udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (udpecho_raw_pcb != NULL) {
    err_t err;

    /* lwip/src/core/udp.c add udp_pcb to udp_pcbs */
    err = udp_bind(udpecho_raw_pcb, IP_ANY_TYPE, 53);
    if (err == ERR_OK) {
      /**
       * lwip/src/core/udp.c
       * @ingroup udp_raw
       * Set a receive callback for a UDP PCB
       *
       * This callback will be called when receiving a datagram for the pcb.
       *
       * @param pcb the pcb for which to set the recv callback
       * @param recv function pointer of the callback function
       * @param recv_arg additional argument to pass to the callback function
       */
      udp_recv(udpecho_raw_pcb, udpecho_raw_recv, NULL);
    } else {
      /* abort? output diagnostic? */
    }
  } else {
    /* abort? output diagnostic? */
  }
}

#endif /* LWIP_UDP */
