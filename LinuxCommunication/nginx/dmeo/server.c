#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

const unsigned int serv_port = 9000; //服务器要监听的端口

int main(int argc, char* const argv)
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);//创建服务器的套接字

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	//设置要监听的地址和端口
	serv_addr.sin_family = AF_INET;//IPV4
	serv_addr.sin_port = htons(serv_port);//绑定端口号
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);//监听所有IP地址

	bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));//绑定服务器地址结构体
	listen(listenfd, 32); //可以积压的请个个数为32

	int connfd;//真正用于通信的套接字
	const char* pContent = "server send to client";

	for (;;)
	{
		connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);//卡在这里知道有客服端连接上来

		write(connfd, pContent, strlen(pContent));

		close(connfd);
	}

	close(listenfd);

	return 0;
}