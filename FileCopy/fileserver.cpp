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
#include <iomanip>
#include "packets.h"
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


bool recvInitial_sendAck(C150DgmSocket *sock,char *targetDir,int fileNastiness);
bool recvFile_sendAcks(C150DgmSocket *sock,string fileName,int numPackets,char *targetDir,int fileNastiness);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
void recvInitial_sendSHA(C150DgmSocket *sock,char *targetDir);
void recvStatus_sendConfirmation(string fileName,C150DgmSocket *sock,int attempt,const Packet &shaPacket,char* targetDir);
void recvWindow(C150DgmSocket *sock,string fileName,vector<FilePacket*> &receivedPackets,int numPackets);
string toHex(const string& raw);
void computeAck(int &lastAck,vector<FilePacket*> &receivedPackets);
void sendAck(C150DgmSocket *sock, string fileName, int lastAck);
bool writeToFile(vector<FilePacket*> &receivedPackets, string fileName, char *targetDir, int fileNastiness);
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
      int fileNastiness = atoi(argv[fileNastinessArg]);

       // Open the target directory
        DIR *TARGET = opendir(argv[targetDirArg]);
        if (TARGET == NULL) {
            fprintf(stderr,"Error opening target directory %s\n", argv[targetDirArg]);     
            exit(8);
        }

        c150debug->setIndent("    ");             
        c150debug->printf(C150APPLICATION,"Creating C150NastyDgmSocket Server(nastiness=%d)",
                           networkNastiness);
        C150DgmSocket *sock = new C150NastyDgmSocket(networkNastiness);
        sock->turnOnTimeouts(1500);
        c150debug->printf(C150APPLICATION,"Ready to accept messages");
        *GRADING << "Server socket created"<<endl;
       
         // Bind the socket to the appropriate port


    try{
            while(1){
                // recvInitial_sendSHA(sock,argv[targetDirArg]);
                /* 
                  recvInitial packet from client - with filename and num packets
                   andsendInitial_AcK
                  RECVFile_sendAcks
                  endtoendCheck
                 */
                  // Leads to file
                  bool fileTransferComplete = recvInitial_sendAck(sock,argv[targetDirArg],fileNastiness);
                  printf("File transfer completed %d, beginning end to end check: \n", fileTransferComplete);
                  if (fileTransferComplete){
                    *GRADING << "File transfer complete, waiting for end-to-end check"<<endl;
                    recvInitial_sendSHA(sock,argv[targetDirArg]);
                    *GRADING << "End to end check complete for file transfer"<<endl;

                  }
                  else{
                    *GRADING << "File transfer failed, waiting for next file"<<endl;
                    continue; // wait for next file
               }
        }
      }
    

    catch(C150NetworkException& e) {

        *GRADING << "Caught C150NetworkException: " << e.formattedExplanation() <<endl;
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

/* recvInitial_sendAck(C150DgmSocket *sock,char *targetDir):
      Recieve a FilePacket of type INITIAL that contains filename that the client will send
      and number of packets to expect .
      Send back an INITIAL_ACK packet to client confirming the filename and num packets.

      Calls recvFile_sendAcks with the appropriate arguments - filename , number of packets
      or recursively calls itself 
  */
 bool recvInitial_sendAck(C150DgmSocket *sock,char *targetDir,int fileNastiness)
 { 
   char initialPacket[sizeof(FilePacket)];
   ssize_t initialLength = sock->read(initialPacket, sizeof(initialPacket));
    if (sock->timedout()) {
       // *GRADING << "Timeout waiting for INITIAL packet, retrying"<<endl;
       return recvInitial_sendAck(sock,targetDir,fileNastiness);
    }

    if (initialLength != sizeof(initialPacket)) {
        return recvInitial_sendAck(sock,targetDir,fileNastiness);
      }
  
      FilePacket initial = *(FilePacket*)initialPacket;
      if (initial.type != INITIAL) {
          *GRADING << "Received non-initial packet, discarding and retrying"<<endl;
         return recvInitial_sendAck(sock,targetDir,fileNastiness);
      }
      string fileName = string(initial.filename);
      int numPackets;
      memcpy(&numPackets, initial.content, sizeof(numPackets));
      *GRADING << "Received INITIAL for file: " << fileName << " with expected packets: " << numPackets << endl;
  
      //Send back INITIAL_ACK Packet to client
      char numPacketsContent[4];
      memcpy(numPacketsContent, &numPackets, sizeof(numPackets));
      FilePacket initialAck(INITIAL_ACK, 1, fileName.c_str(), numPacketsContent, sizeof(numPacketsContent));
      sock -> write((char*)&initialAck, sizeof(initialAck));
      sock -> write((char*)&initialAck, sizeof(initialAck));
      sock -> write((char*)&initialAck, sizeof(initialAck));
      *GRADING << "Sent INITIAL_ACK for file: " << fileName << " with expected packets: " << numPackets << endl;
  
      //Call recvFile_sendAcks with appropriate arguments
      return recvFile_sendAcks(sock,fileName,numPackets,targetDir,fileNastiness);
 }

//-------------------------------------------------------------------------------

/* recvFile_sendAcks(C150DgmSocket *sock,string fileName,int numPackets,char *targetDir):
     Receive FilePacket of type DATA from client , store content in vector
     Send back ACK packets after reading a window of packets
  */
  bool recvFile_sendAcks(C150DgmSocket *sock,string fileName,int numPackets,char *targetDir,int fileNastiness)
  { 
    *GRADING << "File: " << fileName << " starting to receive file" << endl;
    vector<FilePacket*> receivedPackets(numPackets + 1, nullptr); // index 0 unused
    int lastAck = 0;

    while (lastAck < numPackets){
      recvWindow(sock, fileName, receivedPackets, numPackets);
      computeAck(lastAck, receivedPackets);
      sendAck(sock, fileName, lastAck);
    }
    return writeToFile(receivedPackets, fileName, targetDir, fileNastiness); //with TMP suffix
     

    //End To End Check after this
  }

// Receive a window of packets from the client and store in recievedPackets vector
void recvWindow(C150DgmSocket *sock,string fileName,vector<FilePacket*> &receivedPackets,int numPackets)
{
  for (int i = 1; i <= 10; i++) {
    char dataPacket[sizeof(FilePacket)];
    ssize_t dataLength = sock->read(dataPacket, sizeof(dataPacket));
    if (sock->timedout()) {
      // *GRADING << "Timeout waiting for DATA packet, discarding"<<endl;
      continue;
    }
    if (dataLength != sizeof(dataPacket)) {
      *GRADING << "Invalid DATA packet length, discarding"<<endl;
     // continue;
    }

    char *data = new char[sizeof(FilePacket)];
    memcpy(data, dataPacket, sizeof(FilePacket));

    FilePacket *packet = (FilePacket*)data;
    int seqNum = (int)packet->sequenceNumber;
    if (packet->type != DATA || string(packet->filename) != fileName || seqNum > numPackets) {
      delete packet;
      continue;
  }

  printf("Received packet SeqNum: %d for file: %s with content length: %d\n", seqNum, fileName.c_str(), packet->contentLength);
  receivedPackets[seqNum] = packet;
  *GRADING << "Received DATA packet SeqNum: " << seqNum << " for file: " << fileName << endl;
  }

}

void computeAck(int &lastAck,vector<FilePacket*> &receivedPackets)
{
  int totalPackets = receivedPackets.size() - 1; // index 0 unused
  for (int i = lastAck + 1; i <= totalPackets; i++) {
    if (receivedPackets[i] == nullptr) {
      break;
    }
    lastAck = i;
  }
}

void sendAck(C150DgmSocket *sock, string fileName, int lastAck)
{ 
  FilePacket ackPacket(ACK, (uint32_t)lastAck, fileName.c_str(), "", 0);
  sock -> write((char*)&ackPacket, sizeof(ackPacket));
  *GRADING << "Sent ACK for SeqNum: " << lastAck << " for file: " << fileName << endl;

}

bool writeToFile(vector<FilePacket*> &receivedPackets, string fileName, char *targetDir, int fileNastiness)
{
  string tempFileName = makeFileName(string(targetDir), fileName + ".TMP");
  NASTYFILE outputFile(fileNastiness);  
  //
    // do an fopen on the output file
    //
  outputFile.fopen(tempFileName.c_str(), "wb");

  // Allocate a buffer to hold the contents
  // Write the vector packets to the buffer

  size_t fileLength = 0;

    for (size_t i = 1; i < receivedPackets.size(); i++) {
        if (receivedPackets[i] == nullptr) {
          
            return false;
        }
        fileLength += receivedPackets[i]->contentLength;
    }

  printf("File length to write for File: %s is %zu\n", fileName.c_str(), fileLength);
  char *buffer = (char *) malloc(fileLength);
  size_t offset = 0;
    for (size_t i = 1; i < receivedPackets.size(); i++) {
        printf("Writing packet SeqNum: %zu of content length: %d to buffer\n", i, receivedPackets[i]->contentLength);
        memcpy(buffer + offset, receivedPackets[i]->content, receivedPackets[i]->contentLength);
        offset += receivedPackets[i]->contentLength;
        delete receivedPackets[i];
        receivedPackets[i] = nullptr;
    }
  
    //
    // Write the whole file
    //
    size_t len = outputFile.fwrite(buffer, 1, fileLength);
    if (len != fileLength) {
      cerr << "Error writing file " << tempFileName << 
	      "  errno=" << strerror(errno) << endl;
      exit(16);
    }
  
    if (outputFile.fclose() == 0 ) {
      // *GRADING << "Finished writing file " << tempFileName <<endl;
    } else {
      cerr << "Error closing output file " << tempFileName << 
	      " errno=" << strerror(errno) << endl;
      exit(16);
    }

    //
    // Free the input buffer to avoid memory leaks
    //
    free(buffer);
  
    *GRADING << "File: " << fileName << " received, beginning end-to-end check" << endl;
    return true;
}



/* End to End Check Functions */
void recvInitial_sendSHA(C150DgmSocket *sock,char *targetDir) {
         
           char initialPacket[sizeof(Packet)];
           ssize_t initialLength = sock->read(initialPacket, sizeof(initialPacket));  

           if (sock->timedout()) {
               *GRADING << "Timeout waiting for INITIATE end to end packet, retrying"<<endl;
                recvInitial_sendSHA(sock,targetDir);
           }
           printf("packet recvd\n%s", initialPacket);
            if (initialLength != sizeof(initialPacket)) {
               printf("Invalid Read , retrying\n");
               recvInitial_sendSHA(sock,targetDir);
               // c150debug->printf(C150APPLICATION,"Read wrong length message, trying again");
               // *GRADING << "Read wrong length message, terminating"<<endl;
                //throw C150NetworkException("Unexpected length read in server");
                
            }

            Packet initial = *(Packet*)initialPacket;
            printf("Type: %d\n", initial.type);
            if (initial.type != INITIATE) {
                *GRADING << "Received non-initiate packet, discarding and retrying"<<endl;
                recvInitial_sendSHA(sock,targetDir);
            }
            string fileName = string(initial.filename);
            *GRADING << "Received INITIATE for file: " << fileName << endl;

            //Compute SHA1 for file and send to client
      string filename_tmp = fileName + ".TMP";
      char filePath[256];
      snprintf(filePath, sizeof(filePath), "%s/%s", targetDir, filename_tmp.c_str());
      unsigned char obuf[20];
      ifstream t(filePath);
      if (!t.is_open()) {
        // File does not exist, set SHA to all zeros
        memset(obuf, 0, sizeof(obuf));
        *GRADING << "File not found: " << fileName << ", sending impossible SHA"<<endl;
      } else {
        stringstream buffer;
        buffer << t.rdbuf();
        SHA1((const unsigned char *)buffer.str().c_str(), 
          buffer.str().length(), obuf);

         // *GRADING << "File: " << fileName << " received, beginning end-to-end check"<<endl;
          *GRADING << "Computing SHA1 for file: " << fileName << endl;
      }
      // Convert SHA1 to hex string for logging
      string sha1Hex = toHex(string((char*)obuf, sizeof(obuf)));
      printf("SHA1 being sent for file %s is %s\n", fileName.c_str(), sha1Hex.c_str());
      Packet shaPacket(SHA, fileName.c_str(), (char*)obuf);
      
     /// c150debug->printf(C150APPLICATION,"%s: Sending SHA1 for file: \"%s\"",
             // argv[0],fileName);
      *GRADING << "Sending SHA1 to client for file: " << fileName << endl;
      sock -> write((char*)&shaPacket, sizeof(shaPacket));
       recvStatus_sendConfirmation(fileName,sock,1,shaPacket,targetDir);



}

//Also retry sending SHA Packet if read for sha check fails 
void recvStatus_sendConfirmation(string fileName,C150DgmSocket *sock,int attempt,const Packet &shaPacket,char* targetDir)
{  
    if (attempt > 5) {
        printf("Max attempts reached , terminating server\n");
        throw C150NetworkException("Maximum attempts reached for recvStatus_sendConfirmation");
    }
    if(attempt > 1){
      *GRADING << "ReSending SHA1 to client for file: " << fileName << endl;
      sock -> write((char*)&shaPacket, sizeof(shaPacket));
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
                *GRADING << "Received invalid filename or packet type,retrying read"<<endl;
                numRetries++;
                continue;
            }

            char* statusContent = status.content;
            if(strcmp(statusContent, "SUCCESS") == 0){
                *GRADING << "File: " << fileName << " end-to-end check succeeded"<<endl;
                string filename_tmp = makeFileName(string(targetDir), fileName + ".TMP");
                string filename_final = makeFileName(string(targetDir), fileName);
                rename(filename_tmp.c_str(), filename_final.c_str());
            } else if (strcmp(statusContent, "FAILURE") == 0) {
                *GRADING << "File: " << fileName << " end-to-end check failed"<<endl;
                string filename_tmp = makeFileName(string(targetDir), fileName + ".TMP");
                remove(filename_tmp.c_str());
            } else {
                *GRADING << "Received invalid status content, retrying read"<<endl;
                numRetries++;
                continue;
            }

            
            //Send Confirmation Packet
            Packet confirmPacket(CONFIRM, fileName.c_str(), "RECEIVED");
            sock -> write((char*)&confirmPacket, sizeof(confirmPacket));
            return;
            // Add another check here ?  
    }
    return recvStatus_sendConfirmation(fileName, sock, attempt + 1,shaPacket,targetDir);

}


string toHex(const string& raw) {
    stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : raw)
        ss << std::setw(2) << (int)(unsigned char)c;
    return ss.str();
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
