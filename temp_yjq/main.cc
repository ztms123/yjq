#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "echo.h"
#include "address.h"
#include "eventloop.h"

using namespace tiny_muduo;


int main( int argc , char* argv[] )
{
	printf("111");
	EventLoop loop;
	Address adds(argv[0]);
	printf("222");
	EchoServer echo(&loop, adds);
	printf("333");
	echo.Start();
	printf("444");
	loop.Loop();
}
