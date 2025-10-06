
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

enum PacketType { INITIATE, SHA, CONFIRM };

struct Packet {
    PacketType type;
    char filename[256];
    char content[21];

    Packet(PacketType t, const char* fname, const char* cont) {
        type = t;
        strncpy(filename, fname, sizeof(filename));
        filename[sizeof(filename)-1] = '\0';
        strncpy(content, cont, sizeof(content));
        content[sizeof(content)-1] = '\0';
        
    }
}__attribute__((packed));

/* int main(){

    Packet p(INITIATE, "example.txt", "examplecontent");
    char buffer[sizeof(Packet)];
    printf("Type: %d\n", p.type);
printf("Filename: %s\n", p.filename);
printf("Content: %s\n", p.content);
    return 0;
 }*/
