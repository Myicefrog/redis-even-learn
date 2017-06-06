#include "server.h"
#include <stdarg.h>

struct redisServer server; 

void serverLog(int level, const char *fmt, ...) 
{
	va_list ap;
    	char msg[LOG_MAX_LEN];

    	va_start(ap, fmt);
    	vsnprintf(msg, sizeof(msg), fmt, ap);
    	va_end(ap);
	printf("serverlog is %s\n",msg);

}

int listenToPort(int port, int *fds, int *count) 
{
    	int j;
	for(j = 0; j < server.bindaddr_count || j == 0; j++)
	{
		if(server.bindaddr[j] == NULL)
		{
			int unsupported = 0;
			fds[*count] = anetTcp6Server(server.neterr,port,NULL,
                		server.tcp_backlog);
			if (fds[*count] != ANET_ERR) 
			{
                		anetNonBlock(NULL,fds[*count]);
                		(*count)++;
			}
			else if(errno == EAFNOSUPPORT)
			{
				unsupported++;
                		serverLog(LL_WARNING,"Not listening to IPv6: unsupproted");
			}
			
			if (*count == 1 || unsupported) {
                	/* Bind the IPv4 address as well. */
                		fds[*count] = anetTcpServer(server.neterr,port,NULL,
                    			server.tcp_backlog);
                		if (fds[*count] != ANET_ERR) {
                    			anetNonBlock(NULL,fds[*count]);
                    			(*count)++;
                		} else if (errno == EAFNOSUPPORT) {
                    			unsupported++;
                    			serverLog(LL_WARNING,"Not listening to IPv4: unsupproted");
                		}
            		}
			if (*count + unsupported == 2) break;
		}
		else if(strchr(server.bindaddr[j],':'))
		{
			fds[*count] = anetTcp6Server(server.neterr,port,server.bindaddr[j],
                		server.tcp_backlog);
		}
		else
		{
			fds[*count] = anetTcpServer(server.neterr,port,server.bindaddr[j],
                		server.tcp_backlog);
		}
		if (fds[*count] == ANET_ERR) 
		{
            		serverLog(LL_WARNING,
                		"Creating Server TCP listening socket %s:%d: %s",
                		server.bindaddr[j] ? server.bindaddr[j] : "*",
                		port, server.neterr);
            		return C_ERR;
        	}
        	anetNonBlock(NULL,fds[*count]);
        	(*count)++;
	}
	return C_OK;
}	
void  initServer(void)
{
	int j;
	server.pid = getpid();
    	server.current_client = NULL;
    	server.clients = listCreate();
    	server.clients_to_close = listCreate();
	server.clients_pending_write = listCreate();
	server.el = aeCreateEventLoop(server.maxclients+CONFIG_FDSET_INCR);
	if (server.port != 0 &&
        	listenToPort(server.port,server.ipfd,&server.ipfd_count) == C_ERR)
        	exit(1);	
	
	    /* Open the listening Unix domain socket. */
    	if (server.unixsocket != NULL)
	{
        	unlink(server.unixsocket); /* don't care if this fails */
        	server.sofd = anetUnixServer(server.neterr,server.unixsocket,
            	server.unixsocketperm, server.tcp_backlog);
        	if (server.sofd == ANET_ERR)
	 	{
            		serverLog(LL_WARNING, "Opening Unix socket: %s", server.neterr);
            		exit(1);
        	}
        	anetNonBlock(NULL,server.sofd);
    	}

	    /* Abort if there are no listening sockets at all. */
    	if (server.ipfd_count == 0 && server.sofd < 0) 
	{
        	serverLog(LL_WARNING, "Configured to not listen anywhere, exiting.");
        	exit(1);
    	}

    	/* Create an event handler for accepting new connections in TCP and Unix
     	* domain sockets. */
    	for (j = 0; j < server.ipfd_count; j++) 
	{
        	if (aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE,
            		acceptTcpHandler,NULL) == AE_ERR)
            	{
                	printf(
                    		"Unrecoverable error creating server.ipfd file event.\n");
            	}
    	}

	
}

void initServerConfig(void) {
    int j;

    server.port = CONFIG_DEFAULT_SERVER_PORT;
    server.tcp_backlog = CONFIG_DEFAULT_TCP_BACKLOG;
    server.bindaddr_count = 0;
    server.unixsocket = NULL;
    server.unixsocketperm = CONFIG_DEFAULT_UNIX_SOCKET_PERM;
    server.ipfd_count = 0;
    server.sofd = -1;

}




void beforeSleep(struct aeEventLoop *eventLoop) 
{
    	UNUSED(eventLoop);
	/* Handle writes with pending output buffers. */
    	handleClientsWithPendingWrites();
	
}

int main()
{
	printf("start\n");
	initServerConfig();
	initServer();
	aeSetBeforeSleepProc(server.el,beforeSleep);	
	aeMain(server.el);
    	aeDeleteEventLoop(server.el);
	printf("End\n");
    	return 0;
}
