/* Author : Jay Choudhary
    Fall 2025 CS117 : Distributed Systems FileCopy 
   Purpose : Server program that receives files using UDP . 
  */
//------------------------------------------------------


#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>

#include "endToendPacket.cpp"
#include "c150grading.h"
//#include "c150dgmsocket.h"
#include "c150nastyfile.h"    
#include "c150nastydgmsocket.h"
#include "c150debug.h"
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

using namespace std;
// Always use namespace C150NETWORK with COMP 150 IDS framework!
using namespace C150NETWORK;
//------------------------------------------------------------------

void checkDirectory(char *dirname);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
string recvInitial_sendSHA(C150DgmSocket *sock,char *targetDir);
void recvStatus_sendConfirmation(string fileName,C150DgmSocket *sock,int attempt);
//------------------------------------------------------------------
const int networkNastinessArg = 1;        // network nastiness is 1st arg
const int fileNastinessArg = 2;           // file nastiness is 2nd arg
const int targetDirArg = 3;               // target directory is 3rd arg
//------------------------------------------------------------------
//                           main program


int main(int argc,char *argv[])
{ 
     GRADEME(argc, argv);
     if(argc != 4){
       fprintf(stderr,"Usage: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
       exit(1);
     }
      setUpDebugLogging("fileserverdebug.txt",argc, argv);
     
     if (strspn(argv[networkNastinessArg], "0123456789") != strlen(argv[networkNastinessArg])) {
            fprintf(stderr,"Network Nastiness %s is not numeric\n", argv[networkNastinessArg]);         
            exit(4);
        }
     if (strspn(argv[fileNastinessArg], "0123456789") != strlen(argv[fileNastinessArg])) {
            fprintf(stderr,"File Nastiness %s is not numeric\n", argv[fileNastinessArg]);         
            exit(4);
        }
      checkDirectory(argv[targetDirArg]);
      int networkNastiness = atoi(argv[networkNastinessArg]);
     // int fileNastiness = atoi(argv[fileNastinessArg]);

       // Open the target directory
        DIR *TARGET = opendir(argv[targetDirArg]);
        if (TARGET == NULL) {
            fprintf(stderr,"Error opening target directory %s\n", argv[targetDirArg]);     
            exit(8);
        }

         //c150debug->setIndent("    ");             
       // c150debug->printf(C150APPLICATION,"Creating C150NastyDgmSocket Server(nastiness=%d)",
                          //networkNastiness);
        C150DgmSocket *sock = new C150NastyDgmSocket(networkNastiness);
        sock->turnOnTimeouts(1500);
        //c150debug->printf(C150APPLICATION,"Ready to accept messages");
        *GRADING << "Server socket created\n";
         // Bind the socket to the appropriate port


    try{
            while(1){
                string fileName = recvInitial_sendSHA(sock,argv[targetDirArg]);
                //Read Status Packet from client
                recvStatus_sendConfirmation(fileName,sock,1);
                 }
        }
    

    catch(C150NetworkException& e) {

        *GRADING << "Caught C150NetworkException: " << e.formattedExplanation() << "\n";
        // Write to debug log
       // c150debug->printf(C150ALWAYSLOG,"Caught C150NetworkException: %s\n",
                         // e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
       // cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() 
                       // << endl;
    }
    closedir(TARGET);
    TARGET = nullptr;

    return 0;
    



}




void checkDirectory(char *dirname) {
  struct stat statbuf;  
  if (lstat(dirname, &statbuf) != 0) {
    fprintf(stderr,"Error stating supplied source directory %s\n", dirname);
    exit(8);
  }

  if (!S_ISDIR(statbuf.st_mode)) {
    fprintf(stderr,"File %s exists but is not a directory\n", dirname);
    exit(8);
  }
}

string recvInitial_sendSHA(C150DgmSocket *sock,char *targetDir) {
  while(1){

           char initialPacket[sizeof(Packet)];
           ssize_t initialLength = sock->read(initialPacket, sizeof(initialPacket));  

           if (sock->timedout()) {
               *GRADING << "Timeout waiting for INITIATE packet, retrying\n";
               continue;
           }
           printf("packet recvd\n%s", initialPacket);
            if (initialLength != sizeof(initialPacket)) {
               printf("Invalid Read , retrying\n");
               continue;
               // c150debug->printf(C150APPLICATION,"Read wrong length message, trying again");
               *GRADING << "Read wrong length message, terminating\n";
                //throw C150NetworkException("Unexpected length read in server");
                
            }

            Packet initial = *(Packet*)initialPacket;
            printf("Type: %d\n", initial.type);
            if (initial.type != INITIATE) {
                *GRADING << "Received non-initiate packet, discarding and retrying\n";
                continue;
            }
            string fileName = string(initial.filename);
            *GRADING << "Received INITIATE for file: " << fileName << "\n";

            //Compute SHA1 for file and send to client
            
      char filePath[256];
      snprintf(filePath, sizeof(filePath), "%s/%s", targetDir, fileName.c_str());
      unsigned char obuf[20];
      ifstream t(filePath);
      if (!t.is_open()) {
        // File does not exist, set SHA to all zeros
        memset(obuf, 0, sizeof(obuf));
        *GRADING << "File not found: " << fileName << ", sending impossible SHA\n";
      } else {
        stringstream buffer;
        buffer << t.rdbuf();
        SHA1((const unsigned char *)buffer.str().c_str(), 
          buffer.str().length(), obuf);

          *GRADING << "File: " << fileName << " received, beginning end-to-end check";
          *GRADING << "Computing SHA1 for file: " << fileName << "\n";
      }
      Packet shaPacket(SHA, fileName.c_str(), (char*)obuf);
     /// c150debug->printf(C150APPLICATION,"%s: Sending SHA1 for file: \"%s\"",
             // argv[0],fileName);
      *GRADING << "Sending SHA1 to client for file: " << fileName << "\n";
      sock -> write((char*)&shaPacket, sizeof(shaPacket));
      return fileName;
            
}

}

void recvStatus_sendConfirmation(string fileName,C150DgmSocket *sock,int attempt)
{  
    if (attempt > 5) {
        printf("Max attempts reached , terminating server\n");
        throw C150NetworkException("Maximum attempts reached for recvStatus_sendConfirmation");
    }
    int numRetries = 0;
    while (numRetries < 5) {
        char statusPacket[sizeof(Packet)];
        ssize_t statusLength = sock->read(statusPacket, sizeof(statusPacket));
        if (statusLength != sizeof(statusPacket)) {
            printf("recvStatus invalid packet:%s\n",statusPacket);
            printf("Invalid Read IN RECV Status , terminating server\n");
            numRetries++;
            continue;
           // throw C150NetworkException("Unexpected length read in server");
        }
            Packet status = *(Packet*)statusPacket;
            if (status.type != SHA || string(status.filename) != fileName) {
                *GRADING << "Received invalid filename or packet type,retrying read";
                numRetries++;
                continue;
            }

            char* statusContent = status.content;
            if(strcmp(statusContent, "SUCCESS") == 0){
                *GRADING << "File: " << fileName << " end-to-end check succeeded";
            } else if (strcmp(statusContent, "FAILURE") == 0) {
                *GRADING << "File: " << fileName << " end-to-end check failed";
            } else {
                *GRADING << "Received invalid status content, retrying read\n";
                numRetries++;
                continue;
            }

            
            //Send Confirmation Packet
            Packet confirmPacket(CONFIRM, fileName.c_str(), "RECEIVED");
            sock -> write((char*)&confirmPacket, sizeof(confirmPacket));
            return;
            // Add another check here ?  
    }
    return recvStatus_sendConfirmation(fileName, sock, attempt + 1);

}


void setUpDebugLogging(const char *logname, int argc, char *argv[]) {

    //   
    //           Choose where debug output should go
    //
    // The default is that debug output goes to cerr.
    //
    // Uncomment the following three lines to direct
    // debug output to a file. Comment them to 
    // default to the console
    //  
    // Note: the new DebugStream and ofstream MUST live after we return
    // from setUpDebugLogging, so we have to allocate
    // them dynamically.
    //
    //
    // Explanation: 
    // 
    //     The first line is ordinary C++ to open a file
    //     as an output stream.
    //
    //     The second line wraps that will all the services
    //     of a comp 150-IDS debug stream, and names that filestreamp.
    //
    //     The third line replaces the global variable c150debug
    //     and sets it to point to the new debugstream. Since c150debug
    //     is what all the c150 debug routines use to find the debug stream,
    //     you've now effectively overridden the default.
    //
    ofstream *outstreamp = new ofstream(logname);
    DebugStream *filestreamp = new DebugStream(outstreamp);
    DebugStream::setDefaultLogger(filestreamp);


    //
    //  Put the program name and a timestamp on each line of the debug log.
    //
    c150debug->setPrefix(argv[0]);
    c150debug->enableTimestamp(); 

    //
    // Ask to receive all classes of debug message
    //
    // See c150debug.h for other classes you can enable. To get more than
    // one class, you can or (|) the flags together and pass the combined
    // mask to c150debug -> enableLogging 
    //
    // By the way, the default is to disable all output except for
    // messages written with the C150ALWAYSLOG flag. Those are typically
    // used only for things like fatal errors. So, the default is
    // for the system to run quietly without producing debug output.
    //
    c150debug->enableLogging(C150APPLICATION | C150NETWORKTRAFFIC | 
                             C150NETWORKDELIVERY); 
}
