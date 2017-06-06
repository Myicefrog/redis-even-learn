#include "server.h"

#define MAX_ACCEPTS_PER_CALL 1000
int writeToClient(int fd, client *c, int handler_installed) 
{
    	ssize_t nwritten = 0, totwritten = 0;
    	size_t objlen;
    	size_t objmem;

    	nwritten = write(fd,c->buf+c->sentlen,c->bufpos-c->sentlen);
	return C_OK;
}

void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) 
{
    UNUSED(el);
    UNUSED(mask);
    writeToClient(fd,privdata,1);
}

int handleClientsWithPendingWrites(void) 
{
    listIter li;
    listNode *ln;
    int processed = listLength(server.clients_pending_write);
    listRewind(server.clients_pending_write,&li);
    while((ln = listNext(&li))) 
    {
        client *c = listNodeValue(ln);
        c->flags &= ~CLIENT_PENDING_WRITE;
        listDelNode(server.clients_pending_write,ln);

        /* Try to write buffers to the client socket. */
        if (writeToClient(c->fd,c,0) == C_ERR) continue;
	int result = aeCreateFileEvent(server.el, c->fd, AE_WRITABLE,
                sendReplyToClient, c);

    }
    return processed;
}

int prepareClientToWrite(client *c) 
{
	c->flags |= CLIENT_PENDING_WRITE;
        listAddNodeHead(server.clients_pending_write,c);
	return C_OK;
}
void addReplyString(client *c, const char *s, size_t len) 
{
    if (prepareClientToWrite(c) != C_OK) return;
}

void addReplyErrorLength(client *c, const char *s, size_t len) 
{
    addReplyString(c,"-ERR ",5);
    addReplyString(c,s,len);
    addReplyString(c,"\r\n",2);
}

void addReplyError(client *c, const char *err) 
{
    addReplyErrorLength(c,err,strlen(err));
}

int processCommand(client *c) 
{
	addReplyError(c,"only (P)SUBSCRIBE / (P)UNSUBSCRIBE / PING / QUIT allowed in this context");
        return C_OK;
}

void processInputBuffer(client *c) 
{
	while(sdslen(c->querybuf)) 
	{
		processCommand(c);
	}
}

void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) 
{
    	client *c = (client*) privdata;
    	int nread, readlen;
    	size_t qblen;
    	UNUSED(el);
    	UNUSED(mask);

    	readlen = PROTO_IOBUF_LEN;
	qblen = sdslen(c->querybuf);
    	if (c->querybuf_peak < qblen) c->querybuf_peak = qblen;
    	c->querybuf = sdsMakeRoomFor(c->querybuf, readlen);
    	nread = read(fd, c->querybuf+qblen, readlen);
	sdsIncrLen(c->querybuf,nread);
	processInputBuffer(c);	
}

client *createClient(int fd) 
{
    client *c = zmalloc(sizeof(client));

    /* passing -1 as fd it is possible to create a non connected client.
     * This is useful since all the commands needs to be executed
     * in the context of a client. When commands are executed in other
     * contexts (for instance a Lua script) we need a non connected client. */
    if (fd != -1) {
        anetNonBlock(NULL,fd);
        anetEnableTcpNoDelay(NULL,fd);
        if (aeCreateFileEvent(server.el,fd,AE_READABLE,
            readQueryFromClient, c) == AE_ERR)
        {
            close(fd);
            zfree(c);
            return NULL;
        }
    }

    c->fd = fd;
    c->bufpos = 0;
    c->querybuf = sdsempty();
    c->querybuf_peak = 0;
    c->reqtype = 0;
    c->argc = 0;
    c->multibulklen = 0;
    c->bulklen = -1;
    c->sentlen = 0;
    c->flags = 0;
    c->authenticated = 0;
    c->replstate = REPL_STATE_NONE;
    c->repl_put_online_on_ack = 0;
    c->reploff = 0;
    c->repl_ack_off = 0;
    c->repl_ack_time = 0;
    c->slave_listening_port = 0;
    c->slave_ip[0] = '\0';
    c->slave_capa = SLAVE_CAPA_NONE;
    c->reply = listCreate();
    c->reply_bytes = 0;
    c->obuf_soft_limit_reached_time = 0;
    c->btype = BLOCKED_NONE;
    c->woff = 0;
    c->peerid = NULL;
    return c;
}


static void acceptCommonHandler(int fd, int flags, char *ip) 
{
    	client *c;
    	if ((c = createClient(fd)) == NULL) 
	{
        	serverLog(LL_WARNING,
            	"Error registering fd event for the new client: %s (fd=%d)",
            		strerror(errno),fd);
        	close(fd); /* May be already closed, just ignore errors */
        	return;
    	}
}

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);

    while(max--) {
        cfd = anetTcpAccept(server.neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                serverLog(LL_WARNING,
                    "Accepting client connection: %s", server.neterr);
            return;
        }
        serverLog(LL_VERBOSE,"Accepted %s:%d", cip, cport);
        acceptCommonHandler(cfd,0,cip);
    }
}
