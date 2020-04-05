#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

const unsigned int serv_port = 9000; //客户端需要连接的服务器端口
const char* const serv_ip = "192.168.8.88"; //先固定，我虚拟机上的IP地址

int main(int argc, char* const argv)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	//设置需要连接到的服务器信息
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(serv_port);

	if (inet_pton(AF_INET, serv_ip, &serv_addr.sin_addr) <= 0)
	{
		printf("inet_pton failed!\n");
		exit(1);
	}

	//连接到服务器
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("connect to server failed!\n");
		exit(1);
	}

	int n;
	char reviceLine[256 + 1];
	while ((n = read(sockfd, reviceLine, 256)) > 0)
	{
		reviceLine[n] = 0;
		printf("content: %s\n", reviceLine);
	}

	close(sockfd);

	return 0;
}