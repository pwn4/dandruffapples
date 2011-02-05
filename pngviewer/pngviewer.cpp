/*/////////////////////////////////////////////////////////////////////////////////////////////////
 Regionserver program
 This program communications with clients, controllers, PNGviewers, other regionservers, and clockservers.
 //////////////////////////////////////////////////////////////////////////////////////////////////*/
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "../common/ports.h"
#include "../common/timestep.pb.h"
#include "../common/net.h"
#include "../common/messagewriter.h"
#include "../common/messagereader.h"
#include "../common/except.h"

using namespace std;

int main(int argc, char* argv[])
{
	printf("test");

}
