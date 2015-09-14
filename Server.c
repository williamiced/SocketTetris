#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <pty.h>

#define MAX_PLAYER 5
#define ERROR_STR_MAX 100

pthread_t thrId[MAX_PLAYER];
int seat[MAX_PLAYER] = {0};
int socketName[MAX_PLAYER] = {0};

struct threadInfo
{
	int ptyFd;
	int seatIndex;
};

void ErrorSend(char Str[ERROR_STR_MAX])
{
	write(2, Str, strlen(Str));
	exit(-1);
}

void sig_handler(int signo)
{
	signal(SIGPIPE, sig_handler);
}

void WaiterIn(void* thrInfo)
{
	int ptyFd = ((struct threadInfo*)thrInfo)-> ptyFd;
	int index = ((struct threadInfo*)thrInfo)-> seatIndex;
	ssize_t recvValue;
	ssize_t writeValue;
	char keypr;
	char buf[3];
	/* Do serve here */ 
	while(1)
	{
		recvValue = recv(socketName[index], &keypr, sizeof(char), 0);
		if(recvValue < 0) /* If an error occur */
		{	/* Assume that the reason is the connection shut down */
			seat[index] = 0;
			pthread_exit((void*) 0);
		}
		/* Do sth here */
		if(keypr == '@')
		{
			buf[0] = (char)27;
			buf[1] = '[';
			buf[2] = 'A';
			writeValue = write(ptyFd, buf, 3);
		}
		else if(keypr == '#')
		{
			buf[0] = (char)27;
			buf[1] = '[';
			buf[2] = 'B';
			writeValue = write(ptyFd, buf, 3);
		}
		else if(keypr == '$')
		{
			buf[0] = (char)27;
			buf[1] = '[';
			buf[2] = 'C';
			writeValue = write(ptyFd, buf, 3);
		}
		else if(keypr == '%')
		{
			buf[0] = (char)27;
			buf[1] = '[';
			buf[2] = 'D';
			writeValue = write(ptyFd, buf, 3);
		}
		else
			writeValue = write(ptyFd, &keypr, recvValue);
		if(writeValue < 0)
		{
			seat[index] = 0;
			pthread_exit((void*) 0);
		}
	}
}

void WaiterOut(void* thrInfo)
{
	int ptyFd = ((struct threadInfo*)thrInfo)-> ptyFd;
	ssize_t recvValue;
	ssize_t sendValue;
	char keypr;
	int i;
	while(1)
	{
		recvValue = read(ptyFd, &keypr, sizeof(char));
		if(recvValue < 0) /* If an error occur */	/* Assume that the reason is the connection shut down */
			pthread_exit((void*) 0);
		for(i=0; i<MAX_PLAYER; i++)
			if(seat[i] != 0)
				sendValue = send(socketName[i], &keypr, recvValue, 0);
	}
}

int AskSeat(int seat[MAX_PLAYER])
{
	int i;
	for(i=0; i<MAX_PLAYER; i++)
		if(seat[i] == 0)
			return i;
	return -1; /* There is no seat */
}

int MakeSocket(int PortNum)
{
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in ServerAddr;

	if(socketFd < 0)
		ErrorSend("Making socket error\n");
		
	/* Initialization */
	bzero(&ServerAddr, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(PortNum);
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(socketFd, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) < 0)
		ErrorSend("Binding socket error\n");
	return socketFd;
}

int main(int argc, char* argv[])
{
	signal(SIGPIPE, sig_handler);

	int socketWait;
	int socketClient;
	int seatIndex;
	int ptyFd;	
	int numPlayer = 0;
	int PortNum = 0;
	int i;

	char StrPort[] = "-port";

	fd_set socketSet;
	pid_t childId;
	socklen_t clientAddrLen;
	pthread_t outputThr;

	struct sockaddr clientAddr;
	struct threadInfo thrInfo;
	struct winsize termWindowSize;
	ioctl(1, TIOCGWINSZ, &termWindowSize);

	/* Set up Pty */
	childId = forkpty(&ptyFd, NULL, NULL, &termWindowSize);
	if(childId < 0)
		ErrorSend("Fork PTY error\n");
	else if(childId == 0)
	{ /* If Child */	
		if(execv("./vitetris-0.57/tetris", NULL) < 0)
			ErrorSend("EXECV error\n");
	}
	
	/* If Parent */
	/* Read -port */
	for(i=1; i<argc; i++)
		if(!strcmp(argv[i], StrPort))
			PortNum = atoi(argv[i+1]);
	
	if(PortNum == 0)
		ErrorSend("Lack of -port or Port number\n");		

	socketWait = MakeSocket(PortNum);
	listen(socketWait, MAX_PLAYER);
	thrInfo.ptyFd = ptyFd;

	pthread_create(&outputThr, NULL, WaiterOut, (void*)&thrInfo);

	while(1)
	{
		printf("Connecting...\n");
		for(i=0;i<MAX_PLAYER;i++)
			printf("%d ", seat[i]);
		printf("\n");
		seatIndex = AskSeat(seat); /* Ask a seat to be */
		if(seatIndex == -1) /* If there is no seat */
		{
			sleep(1); /* pthread wait or continue */
			continue;
		}
		else
		{
			kill(childId, SIGCHLD);
			socketClient = accept(socketWait, (struct sockaddr*)&clientAddr, &clientAddrLen); /* To wait for connection */
			seat[seatIndex] = 1;
		}
		socketName[seatIndex] = socketClient;
		thrInfo.seatIndex = seatIndex;
		pthread_create(&thrId[seatIndex], NULL, WaiterIn, (void*)&thrInfo);
		if(waitpid(childId, 0, WNOHANG) == childId) /* If tetris is closed */
		{	
			printf("Tetris is closed\n");
			break;
		}
	}

	return 1; /* Execute succesfully */
}

