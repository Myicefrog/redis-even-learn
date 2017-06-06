#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

void test_connect()
{
	unsigned  short port = 6379;
	char* server_ip = "192.168.21.130";
	int sockfd;
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
	{
		perror("socket failed");
		exit(-1);
	}

	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET,server_ip,&server_addr.sin_addr);

	int err_log = connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr));

	if(err_log !=0)
	{
		perror("connect fail");
		close(sockfd);
		exit(-1);
	}

	char send_buf[100] = "this is for test";
	send(sockfd,send_buf,strlen(send_buf),0);
	char revc_buf[100] = {0};
	int num = recv(sockfd,revc_buf,strlen(revc_buf),0);
	if(num > 0)
	printf("I got recv %s\n",revc_buf);
	close(sockfd);
}

int main()
{
	int count = 1000;
	while(count--)
		test_connect();
	return 1;
}
