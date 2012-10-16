/*
 * Copyright 2011 self.disconnect
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include "httpd.h"
#include "apr_thread_proc.h"
#include "websocket_plugin.h"

typedef struct _MosquittoData {
  const WebSocketServer *server;
  apr_pool_t *pool;
  apr_thread_t *thread;
  int counter;
  int active;
  int sockfd;
} MosquittoData;

void* APR_THREAD_FUNC mosquitto_run(apr_thread_t *thread, void *data)
{
  char buffer[1024];
  MosquittoData *dib = (MosquittoData *) data;

  if (dib != NULL) {
    /* Keep sending messages as long as the connection is active */
    while (dib->active) {
      int bread = recv(dib->sockfd,buffer,sizeof(buffer),0);
      if (bread && dib->server)
         dib->server->send(dib->server, MESSAGE_TYPE_BINARY, (unsigned char *)buffer,bread);
     else
		 dib->active=0;
     }
  }
  return NULL;
}

void * CALLBACK mosquitto_on_connect(const WebSocketServer *server)
{
  MosquittoData *dib = NULL;
  struct sockaddr_in pin;
  struct hostent *hp;

  if ((server != NULL) && (server->version == WEBSOCKET_SERVER_VERSION_1)) {
    /* Get access to the request_rec strucure for this connection */
    request_rec *r = server->request(server);

    if (r != NULL) {
      apr_pool_t *pool = NULL;
      size_t i, count = server->protocol_count(server);

      /* Only support "mqtt" */
      for (i = 0; i < count; i++) {
        const char *protocol = server->protocol_index(server, i);

        if ((protocol != NULL) &&
            (strcmp(protocol, "mqtt") == 0)) {
          /* If the client can speak the protocol, set it in the response */
          server->protocol_set(server, protocol);
          break;
        }
      }
      /* If the protocol negotiation worked, create a new memory pool */
      if ((i < count) &&
          (apr_pool_create(&pool, r->pool) == APR_SUCCESS)) {
        /* Allocate memory to hold mosquitto state */
        if ((dib = (MosquittoData *) apr_palloc(pool, sizeof(MosquittoData))) != NULL) {
          apr_thread_t *thread = NULL;
          apr_threadattr_t *thread_attr = NULL;

          dib->server = server;
          dib->pool = pool;
          dib->thread = NULL;
          dib->counter = 0;
          dib->active = 1;

		  /* fill in the socket structure with host information */
		  memset(&pin, 0, sizeof(pin));
		  pin.sin_family = AF_INET;
		  hp = gethostbyname("ha-12.dk.eradus.com"); // we need to configure it later
		  pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
		  pin.sin_port = htons(1883);

		  /* grab an Internet domain socket */
		  dib->sockfd = socket(AF_INET, SOCK_STREAM, 0);
          connect(dib->sockfd,(struct sockaddr *)  &pin, sizeof(pin));


          /* Create a non-detached thread that will perform the work */
          if ((apr_threadattr_create(&thread_attr, pool) == APR_SUCCESS) &&
              (apr_threadattr_detach_set(thread_attr, 0) == APR_SUCCESS) &&
              (apr_thread_create(&thread, thread_attr, mosquitto_run, dib, pool) == APR_SUCCESS)) {
            dib->thread = thread;
            /* Success */
            pool = NULL;
          } else {
            dib = NULL;
          }
        }
        if (pool != NULL) {
          apr_pool_destroy(pool);
        }
      }
    }
  }
  return dib;
}

static size_t CALLBACK mosquitto_on_message(void *plugin_private, const WebSocketServer *server,
    const int type, unsigned char *buffer, const size_t buffer_size)
{
  MosquittoData *dib = (MosquittoData *) plugin_private;

  if (dib != 0 && dib->sockfd)  // Just pass it on
	send(dib->sockfd, buffer, buffer_size, 0);
  return 0;
}

void CALLBACK mosquitto_on_disconnect(void *plugin_private, const WebSocketServer *server)
{
  MosquittoData *dib = (MosquittoData *) plugin_private;

  if (dib != 0) {
    /* When disconnecting, inform the thread that it is time to stop */
    dib->active = 0;
    if (dib->thread) {
      apr_status_t status;
      /* Wait for the thread to finish */
      status = apr_thread_join(&status, dib->thread);
      close(dib->sockfd);
    }
    apr_pool_destroy(dib->pool);
  }
}

/*
 * Since we are returning a pointer to static memory, there is no need for a
 * "destroy" function.
 */

static WebSocketPlugin s_plugin = {
  sizeof(WebSocketPlugin),
  WEBSOCKET_PLUGIN_VERSION_0,
  NULL, /* destroy */
  mosquitto_on_connect,
  mosquitto_on_message,
  mosquitto_on_disconnect
};

extern EXPORT WebSocketPlugin * CALLBACK mosquitto_init()
{
  return &s_plugin;
}
