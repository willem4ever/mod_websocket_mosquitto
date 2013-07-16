/*
 * Based on an example by self.disconnect
 * (c) Willem4Ever BV 2012
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
#include "http_log.h"
#include "http_config.h"
#include "apr_thread_proc.h"
#include "websocket_plugin.h"

#define HEADER_SIZE 4096

typedef struct _MosquittoData {
  const WebSocketServer *server;
  apr_pool_t *pool;
  apr_thread_t *thread;
  int counter;
  int active;
  apr_socket_t *sockfd;
} MosquittoData;


typedef struct mosquitto_cfg {
  char* broker;
  char* port;
} mosquitto_cfg ;

static const command_rec mosquitto_cmds[] = {
  AP_INIT_TAKE1("MosBroker", ap_set_string_slot,
	(void*) APR_OFFSETOF(mosquitto_cfg, broker), ACCESS_CONF,
	"Broker") ,
  AP_INIT_TAKE1("MosPort", ap_set_string_slot,
	(void*) APR_OFFSETOF(mosquitto_cfg, port), ACCESS_CONF,
	"Port") ,
 	{NULL}	
};

module AP_MODULE_DECLARE_DATA mod_websocket_mosquitto;
				  
static void* mosquitto_create_dir_conf(apr_pool_t* pool, char* x) {
	mosquitto_cfg* dir = apr_pcalloc(pool, sizeof(mosquitto_cfg));

	dir->broker = "ha-12.dk.eradus.com";
	dir->port = "1883";

	return dir ;
}


void* APR_THREAD_FUNC mosquitto_run(apr_thread_t *thread, void *data)
{
	char *buffer;
	char header[HEADER_SIZE];
	apr_size_t bread;
	MosquittoData *dib = (MosquittoData *) data;
	apr_size_t pos;
	apr_size_t value;
	int mult;

	if (dib != NULL) {
		/* Keep sending messages as long as the connection is active */
		bread = HEADER_SIZE;
		pos = 0;
		while (dib->active) {
			/* Read the header */
			value = 0;
			mult = 1;
			if (pos == 0) {
				bread = HEADER_SIZE;
				apr_socket_recv(dib->sockfd, header, &bread);
			}
			pos = 1;
			if (bread && dib->server) {
				do {
					value += (header[pos] & 127) * mult;
					mult *= 128;
				} while (header[pos++] & 128);
				value += pos;
				if (value < HEADER_SIZE) { // short send
					if (value <= bread) { // enough data, so send it
						dib->server->send(dib->server, MESSAGE_TYPE_BINARY, (unsigned char *) header,value);
						if (bread > value) memmove(header, header+value, bread-(value));
						pos = bread - value;
						bread -= value;
					} else { // not enough data, get more
						pos = bread;
						bread = HEADER_SIZE - pos;
						apr_socket_recv(dib->sockfd, header + pos, &bread);
						bread += pos;
						pos = 1;
					}
				} else { //long send
					buffer = malloc(value);
					if (buffer != NULL) {
						memcpy(buffer, header, bread);
						pos = 0;
						do {
							pos += bread;
							bread = value - pos;
							apr_socket_recv(dib->sockfd, buffer+pos, &bread);
							if (bread == 0) break;
						} while (value - (pos + bread));
						if (bread && dib->server) {
							dib->server->send(dib->server, MESSAGE_TYPE_BINARY, (unsigned char *) buffer,value);
							bread = HEADER_SIZE;
							pos = 0;
						} else {
							dib->active = 0;
						}
						free(buffer);
					} else {
						dib->active = 0;
					}
				}
			} else {
				dib->active = 0;
			}
		}
	}
	return NULL;
}

void * CALLBACK mosquitto_on_connect(const WebSocketServer *server)
{
  MosquittoData *dib = NULL;
  apr_sockaddr_t *sa;

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
            (strncmp(protocol, "mqtt",4) == 0)) {
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

		  request_rec *r = server->request(server);
		  mosquitto_cfg* dir = ap_get_module_config(r->per_dir_config, &mod_websocket_mosquitto) ;
		  
		  int rv = apr_sockaddr_info_get(&sa,dir->broker,APR_UNSPEC,atoi(dir->port),APR_IPV6_ADDR_OK ,pool);
		  if (rv)
			  ap_log_error(APLOG_MARK, APLOG_CRIT,0,NULL,"apr_sockaddr_info_get failed #%x",rv);
		  else
			  ap_log_error(APLOG_MARK, APLOG_DEBUG,0,NULL,"Address family %s",sa->family == APR_INET6 ? "IPv6" : "IPv4");

		  rv = apr_socket_create(&dib->sockfd,sa->family, SOCK_STREAM, APR_PROTO_TCP,pool);
		  if (rv) ap_log_error(APLOG_MARK, APLOG_CRIT,0,NULL,"apr_socket_create failed #%x",rv);
		  
		  rv = apr_socket_connect(dib->sockfd,sa);
		  if (rv) ap_log_error(APLOG_MARK, APLOG_CRIT,0,NULL,"apr_socket_connect failed #%x",rv);
		  			
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
  apr_size_t bwritten;
	
  if (dib != 0 && dib->sockfd)  // Just pass it on
	bwritten = buffer_size;
	apr_socket_send(dib->sockfd, (char *)buffer, &bwritten);
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
      apr_socket_close(dib->sockfd);
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
  // ap_log_error(APLOG_MARK, APLOG_CRIT, 0,NULL,"mosquitto_init");
  return &s_plugin;
}

static int mod_mosquitto_method_handler(request_rec * r)
{
    return DECLINED;
}

static void mod_mosquitto_register_hooks(apr_pool_t * p)
{
    ap_hook_handler(mod_mosquitto_method_handler, NULL, NULL,
                    APR_HOOK_LAST);
}


module AP_MODULE_DECLARE_DATA mod_websocket_mosquitto = {
	STANDARD20_MODULE_STUFF,
	mosquitto_create_dir_conf,	/* Create config rec for Directory */
	NULL,						/* Merge config rec for Directory */
	NULL,						/* Create config rec for Host */
	NULL,						/* Merge config rec for Host */
	mosquitto_cmds,				/* Configuration directives */
	mod_mosquitto_register_hooks
};




