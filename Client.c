#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_PLAYER 5
#define ERROR_STR_MAX 100
#define IP_LEN 20

struct threadInfo
{
	int ptyFd;
	int seatIndex;	
	int socket;
};

void WaiterIn(void* info)
{
	ssize_t readValue;
	char keypr;
	char buf[3];
	int escOccur = 0;
	while(1)
	{
		readValue = read(0, &keypr, 1);
		if(escOccur == 0 && keypr == (char)27)
			escOccur = 1;
		else if(escOccur == 0 && keypr != (char)27)
			write(((struct threadInfo*)info) -> socket, &keypr, readValue);
		else if(escOccur == 1 && keypr == '[')
			escOccur = 2;
		else if(escOccur == 1 && keypr != '[')
		{
			buf[0] = (char)27;
			write(((struct threadInfo*)info) -> socket, &buf[0], readValue);
			write(((struct threadInfo*)info) -> socket, &keypr, readValue);
			escOccur = 0;
		}
		else if(escOccur == 2 && (keypr == 'A' || keypr == 'B' || keypr == 'C' || keypr == 'D'))
		{
			if(keypr == 'A')
				keypr = '@';
			else if(keypr == 'B')
				keypr = '#';				
			else if(keypr == 'C')
				keypr = '$';				
			else if(keypr == 'D')
				keypr = '%';
			write(((struct threadInfo*)info) -> socket, &keypr, readValue);			
			escOccur = 0;
		}
		else if(escOccur == 2 && !(keypr == 'A' || keypr == 'B' || keypr == 'C' || keypr == 'D'))
		{
			buf[0] = (char)27;
			buf[1] = '[';
			write(((struct threadInfo*)info) -> socket, &buf[0], readValue);
			write(((struct threadInfo*)info) -> socket, &buf[1], readValue);
			write(((struct threadInfo*)info) -> socket, &keypr, readValue);
			escOccur = 0;
		}

		if(readValue <= 0)
			pthread_exit((void*) 0);
	}
}

void WaiterOut(void* info)
{
	ssize_t readValue;
	char a;
	while(1)
	{
		readValue = read(((struct threadInfo*)info) -> socket, &a, 1);
		if(readValue >0)
			write(1, &a, 1);
		else
			pthread_exit((void*) 0);
	}
}

void ErrorSend(char Str[ERROR_STR_MAX])
{
	write(2, Str, strlen(Str));
	exit(-1);
}

void SetTerm()
{
	struct termios term;
	if(tcgetattr(0, &term) < 0)
		ErrorSend("Get terminal's attribute error\n");
	term.c_lflag &= ~ECHO;
	term.c_lflag &= ~ICANON;
	if(tcsetattr(0, TCSANOW, &term) < 0)
		ErrorSend("Set terminal's attribute error\n");	
}

int MakeSocket(int PortNum, char IPAddr[IP_LEN])
{
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in ServerAddr;

	if(socketFd < 0)
		ErrorSend("Making socket error\n");
		
	/* Initialization */
	bzero(&ServerAddr, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(PortNum);
	inet_pton(AF_INET, IPAddr, &ServerAddr.sin_addr);
	if(connect(socketFd, (struct sockaddr *) &ServerAddr, sizeof(ServerAddr)) < 0)
	{
		SetTerm();
		ErrorSend("Connecting error\n");
	}
	return socketFd;
}

int main(int argc, char* argv[])
{
	int socketFd;
	int PortNum = 0;
	int i;

	char IPAddr[IP_LEN] = "";
	char StrPort[] = "-port";
	char StrIP[] = "-IP";

	for(i=1; i<argc; i++)
	{
		if(!strcmp(argv[i], StrPort))
			PortNum = atoi(argv[i+1]);
		else if(!strcmp(argv[i], StrIP))
			strcpy(IPAddr, argv[i+1]);
	}	
	if(PortNum == 0)
		ErrorSend("Lack of -port or Port number\n");		
	if(strlen(IPAddr) == 0)
		ErrorSend("Lack of -IP or IP address\n");


	SetTerm();
	socketFd = MakeSocket(PortNum, IPAddr);


	pthread_t thrIdin;
	pthread_t thrIdout;
	struct threadInfo thrInfo;
	thrInfo.socket = socketFd;
	pthread_create(&thrIdin, NULL, WaiterIn, (void*)&thrInfo);
	pthread_create(&thrIdout, NULL, WaiterOut, (void*)&thrInfo);	
	pthread_join(thrIdin);
	pthread_join(thrIdout);

}
