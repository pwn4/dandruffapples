//This library contains a set of workhorse functions and their required objects for all entities.
#include "functions.h"

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
#include <stdlib.h>

#include <google/protobuf/message.h>

using namespace std;


//takes a protoID number such as TIMESTEPUPDATE, a pointer to the object to be sent, and returns the string to be sent.

string makePacket(MessageType protoType, void * protoObject)
{
    std::stringstream ss;
    std::string msg;
    
    //add the proto object data
    (* ((google::protobuf::Message *)protoObject)).SerializeToString(&msg);
    
    //add the packet length and proto id to the front
    ss << protoType << '\0' << msg.length() << '\0' << msg;
    
    return ss.str();
}

//takes a string with packetBuffer data, returns packetType=-1 if no packet data in the string
//otherwise returns the google proto object and its type (for casting).
//WARNING: modifies the string given!
protoPacket parsePacket(std::string * packetBuffer)
{
    protoPacket rtn;
    
    std::string token;
    size_t nextDelim;
    int packetsize;
    
    //we may have multiple packets all in the buffer together. Tokenize one. Damn TCP streaming
    nextDelim = (*packetBuffer).find('\0');
    if(nextDelim == string::npos)
    {
        rtn.packetType = -1;
        return rtn;
    }

    //the first string is the proto id.
    token = (*packetBuffer).substr(0, nextDelim);
    rtn.packetType = atoi(token.c_str());
    (*packetBuffer).erase(0, nextDelim+1); //take it off

    //the next string is the packet length
    nextDelim = (*packetBuffer).find('\0');
    token = (*packetBuffer).substr(0, nextDelim);
    packetsize = atoi(token.c_str());
    (*packetBuffer).erase(0, nextDelim+1); //take it off
    
    //we now have our packet. normally use the packettype to identify it, but this is just for now
    rtn.packetData = (*packetBuffer).substr(0, packetsize);
    (*packetBuffer).erase(0, rtn.packetData.length()); //take it off
    ///////////////////////
    
    return rtn;
    
}
