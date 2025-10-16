#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>                // for errno string formatting
#include <cerrno>
#include <cstring>               // for strerro
#include <iostream>               // for cout
#include <fstream>
#include <utility>
#include <vector>
#include "c150nastyfile.h"

using namespace std;

/* Struct declaration for the end to end check Packet */
enum PacketType { INITIATE, SHA, CONFIRM };

struct Packet {
    PacketType type;
    char filename[30];
    char content[21];

    Packet(PacketType t, const char* fname, const char* cont) {
        type = t;
        strncpy(filename, fname, sizeof(filename));
        filename[sizeof(filename)-1] = '\0';
        memcpy(content, cont, sizeof(content));
        content[sizeof(content)-1] = '\0';
        
    }
}__attribute__((packed));

//-----------------------------------------------------------------------------
/* Struct Declaration for the File Transfer Packet */
enum FilePacketType { INITIAL,INITIAL_ACK,DATA,ACK};
struct FilePacket {
    FilePacketType type;
    uint32_t sequenceNumber;
    uint32_t contentLength; 
    char filename[30];
    char content[450];

  FilePacket(FilePacketType t, uint32_t seqNum, const char* fname, const char* cont, size_t contlen) {
    type = t;
    sequenceNumber = seqNum;
    contentLength = contlen;
    strncpy(filename, fname, sizeof(filename));
    filename[sizeof(filename)-1] = '\0';
    memset(content, 0, sizeof(content)); 
    memcpy(content, cont, contlen); // Only copy contlen bytes
}
}__attribute__((packed));

vector<FilePacket> makeFileTransferPackets(string fileName,string dir,int nastiness);
string makeFileName(string dir, string name);
void checkDirectory(char *dirname);
bool isFile(string fname);