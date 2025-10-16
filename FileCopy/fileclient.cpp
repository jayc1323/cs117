/* Author : Jay Choudhary
    Fall 2025 CS117 : Distributed Systems FileCopy 
   Purpose : Client program that copies files to the server using UDP . 
  */
//------------------------------------------------------

#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <openssl/sha.h>
#include "c150nastyfile.h"    
#include "packets.h"
#include "c150grading.h"
//#include "c150dgmsocket.h"
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
#include <iomanip>
#include <sstream>

using namespace std;
// Always use namespace C150NETWORK with COMP 150 IDS framework!
using namespace C150NETWORK;


void setUpDebugLogging(const char *logname, int argc, char *argv[]);
bool beginEndtoEndCheck(char *srcDir,char* fileName,C150DgmSocket *sock,int attempt);
char* initiate_ReceiveSHA(char *fileName,int attempt, C150DgmSocket *sock); //retry 5 times
int compareSHA_ReceiveConfirmation(char* sha,char* srcDir,char *fileName,int attempt,C150DgmSocket *sock); //retry 5 times
string toHex(const string& raw);
bool beginFileTransfer(char *srcDir,char* fileName,int fileNastiness,C150DgmSocket *sock);
void sendWindow(C150DgmSocket *sock,vector<FilePacket> &packets,int base,int windowSize);
int receiveAck(C150DgmSocket *sock, int base, int windowSize,int attempt);
//------------------------------------------------------------------
const int serverArg = 1;                  // server name is 1st arg
const int networkNastinessArg = 2;        // network nastiness is 2nd arg
const int fileNastinessArg = 3;           // file nastiness is 3rd arg
const int sourceDirArg = 4;               // source directory is 4th arg
//------------------------------------------------------------------
//                           main program


int main(int argc,char *argv[])
{ 
     GRADEME(argc, argv);

     // Validate command line arguments
     // Expecting 4 arguments : server , network nastiness , file nastiness , source DIR
     if(argc != 5){
       fprintf(stderr,"Usage: %s <server> <networknastiness> <filenastiness> <srcdir>\n", argv[0]);
       exit(1);
     }
     setUpDebugLogging("fileclientdebug.txt",argc, argv);
     
     if (strspn(argv[networkNastinessArg], "0123456789") != strlen(argv[networkNastinessArg])) {
            fprintf(stderr,"Network Nastiness %s is not numeric\n", argv[networkNastinessArg]);         
            exit(4);
        }
     if (strspn(argv[fileNastinessArg], "0123456789") != strlen(argv[fileNastinessArg])) {
            fprintf(stderr,"File Nastiness %s is not numeric\n", argv[fileNastinessArg]);         
            exit(4);
        }
     checkDirectory(argv[sourceDirArg]);

     //-------------------------------------------------------------------------------
    
     // Create the socket and point to server
        int fileNastiness = atoi(argv[fileNastinessArg]);
        C150DgmSocket *sock = new C150NastyDgmSocket(atoi(argv[networkNastinessArg])); 
       
         
        *GRADING << "Client socket created"<<endl;
        // Tell the DGMSocket which server to talk to
        sock -> setServerName(argv[serverArg]); 
        sock -> turnOnTimeouts(1500);

    //-------------------------------------------------------------------------

    //Call File Transfer
    // Call endtoend check function
   // beginEndtoEndCheck(argv[sourceDirArg],sock);
   // *GRADING << "End to end check complete for directory : " << argv[sourceDirArg] << endl;
    char *srcDir = argv[sourceDirArg];
       DIR *SRC = opendir(srcDir);
        if (SRC == NULL) {
            fprintf(stderr,"Error opening source directory %s\n", srcDir);
            exit(8);
        }

        *GRADING << "Starting end to end check for directory: " << srcDir << endl;

    //END TO END CHECK
    //Initiate end to end check for each file
    // Uses Packet struct from endtoendPacket.cpp for the end to end protocol
          
       try{
         struct dirent *sourceFile;  // Directory entry for source file

          while ((sourceFile = readdir(SRC)) != NULL) {
            // skip the . and .. names
            if ((strcmp(sourceFile->d_name, ".") == 0) ||
            (strcmp(sourceFile->d_name, "..")  == 0 )) 
            continue;          // never copy . or ..

             char* fileName = sourceFile->d_name;
            //Begin File Transfer
            int attempt = 1;
            while (attempt <= 5) {
                        *GRADING << "File: " << fileName << ", beginning transmission, attempt " << attempt << endl;
                        bool success = beginFileTransfer(argv[sourceDirArg], fileName, fileNastiness, sock);
                        if (success) {
                            *GRADING << "File: " << fileName << " transmission complete, waiting for end-to-end check, attempt " << attempt << endl;
                            bool endToEndSuccess = beginEndtoEndCheck(argv[sourceDirArg],fileName,sock,1); //retry 5 times  
                            if (endToEndSuccess){
                                // File transfer and end to end check complete for that file , move to next file
                                *GRADING << "File: " << fileName << " transfer and end-to-end check successfully complete"<<endl;
                                break;

                            }
                            else{
                                *GRADING << "File: " << fileName << " retrying file transfer"<<endl;
                                attempt++;
                                continue; // retry file transfer

                            }

                } else {
                    *GRADING << "File transfer failed for file: " << fileName << " on attempt " << attempt << ", retrying"<<endl;
                    attempt++;
                    continue; 
                }
            } }
        }
    catch(C150NetworkException& e) {
        // Write to debug log
        *GRADING << "Caught C150NetworkException: " << e.formattedExplanation() << endl;

        closedir(SRC);
        exit(4);
    }

  return 0;


}


/* beginEndtoEndCheck(char *srcDir) : 
 Wrapper function for the protocol that
 Receives file name to check at server end 
 and initiates the end to end check - calling the protocol functions on each file
 in that directory.
   */
bool beginEndtoEndCheck(char *srcDir,char* fileName,C150DgmSocket *sock,int attempt)
{  
    char* sha = initiate_ReceiveSHA(fileName,attempt,sock); //retry 5 times
    int success = compareSHA_ReceiveConfirmation(sha,srcDir,fileName,attempt,sock); //retry 5 times
    if (success == 0){
                    *GRADING << "File: " << fileName << " end-to-end check succeeded, attempt "<<attempt<<endl;
                    return true;
                } 
    
        *GRADING << "File: " << fileName << " end-to-end check failed, attempt "<<attempt<<endl;
        return false;

        }


char* initiate_ReceiveSHA(char *fileName,int attempt, C150DgmSocket *sock) {
    if (attempt > 5) {
        throw C150NetworkException("Maximum attempts reached for initiate_ReceiveSHA");
    }
   *GRADING << "File: " << fileName << " transmission complete, waiting for end-to-end check, attempt "<<attempt<<endl;

             //STEP 1: Send INITIATE Packet to server , expecting SHA1 for that file in response .
            Packet initialPacket(INITIATE, fileName, "");
          
            
             *GRADING << "Initiating end to end check for file: " << fileName << endl;
              sock -> write((char*)&initialPacket, sizeof(initialPacket)); 

             
             int readRetry = 0;
             while(readRetry < 5){

              //Expect SHA1 for that file from user
             char sha_buffer[sizeof(Packet)];
            ssize_t readlen = sock->read(sha_buffer, sizeof(sha_buffer));
             
             if(sock->timedout()){
                readRetry++;
                *GRADING << "Timeout waiting for SHA1 from server for file: " << fileName
                 << ", retrying " << readRetry << endl;
                continue;
             }


            if (readlen != sizeof(Packet)) {
                    *GRADING << "Read zero length msg , expecting SHA1"<<endl;
                    readRetry++;
                    continue;
                    //throw C150NetworkException("Unexpected length read in client");
                }
               
               Packet sha = *(Packet*)sha_buffer;
               PacketType header = sha.type;
                if (header != SHA || strcmp(sha.filename, fileName) != 0) {
                     *GRADING << "Received non-SHA packet, expecting SHA1 ,retrying read"<<endl;
                        readRetry++;
                        continue;
                }

               char* sha1content = (char*)malloc(20);
               memcpy(sha1content, sha.content, 20);
               *GRADING << "Received SHA for file: " << fileName<<endl;
               return sha1content;
            }

               // maybe 1st initiate packet wasn't sent , retry
               return initiate_ReceiveSHA(fileName,attempt+1,sock); //retry 5 times


}

int compareSHA_ReceiveConfirmation(char* sha,char* srcDir,char *fileName,int attempt,C150DgmSocket *sock){
    if (attempt > 5) {
        throw C150NetworkException("Maximum attempts reached for compareSHA_ReceiveConfirmation");
    }
    //Compare SHA
      // STEP 2: Compare SHA1 and send STATUS Packet to server, expecting CONFIRM Packet in response
             //Compute SHA1 for file and compare
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "%s/%s", srcDir, fileName);
            unsigned char obuf[20]; 
            ifstream* t = new ifstream(filePath);
            stringstream* buffer = new stringstream;
            *buffer << t->rdbuf();
            SHA1((const unsigned char *)buffer->str().c_str(), 
                (buffer->str()).length(), obuf);
            
            string status;
            string computed_sha = string((char*)obuf,20);
            *GRADING << "SHA received from server for file: " << fileName << " is: " << toHex(string(sha,20)) << endl;
            *GRADING << "Computed SHA for file: " << fileName << " is: " << toHex(computed_sha) << endl;
            int result = memcmp(computed_sha.data(), sha, 20);
             if (result == 0){
                status = "SUCCESS";
                  printf("SHA Check succeeded for file: %s\n", fileName);
                  *GRADING << "SHA Check succeeded for file: " << fileName<<endl;
             }else{
                status = "FAILURE";
                    printf("SHA Check failed for file: %s\n", fileName);
                    *GRADING << "SHA Check failed for file: " << fileName<<endl;
             }
             	delete t;
	            delete buffer;
                
           
             
                //Send STATUS Packet
             Packet statusPacket(SHA, fileName, status.c_str());
             *GRADING << "Sending SHA check result to server for file: " << fileName <<" with result: " << status<<endl;
           
             sock -> write((char*)&statusPacket, sizeof(statusPacket));


             //Wait for Confirmation of Status
             int numRetries = 0;

             while(numRetries < 5){
             char confirmationPacket[sizeof(Packet)];

             // DO a read for confirmation packet
             ssize_t confirmationLength = sock->read(confirmationPacket, sizeof(confirmationPacket));

            // CASE : Read timedout , retry
                if(sock->timedout()){
                    *GRADING << "Timeout waiting for confirmation from server for file: " << fileName 
                    << ", retrying " << attempt << endl; 
                    numRetries++;
                    continue;
                }

                // CASE : Read length 0 , throw exception 
             if (confirmationLength != sizeof(confirmationPacket)) {

                *GRADING<<"Read failed for confirmation , throwing exception"<<endl;
                throw C150NetworkException("Unexpected length read in client");
            }



                Packet confirm = *(Packet*)confirmationPacket;

                // Received Packet but invalid type , discard and retry
                if (confirm.type!=CONFIRM || strcmp(confirm.filename, fileName) != 0) {
                        *GRADING << "Received non-CONFIRM packet, expecting CONFIRM ,retrying read"<<endl;
                        numRetries++;
                        continue;
                 }

                else {
                    
                    // END TO END CHECKING COMPLETE FOR THAT FILE
                    *GRADING << "Received confirmation from server for file: " << fileName<<endl;
                    printf("End to end check complete for file: %s\n", fileName);
                    return status == "SUCCESS" ? 0 : 1; //0 for success , 1 for failure

                 } 
                
             } 
                // maybe SHA status packet wasn't sent , retry
            return compareSHA_ReceiveConfirmation(sha,srcDir,fileName,attempt+1,sock); //retry 5 times

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


string toHex(const string& raw) {
    stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : raw)
        ss << std::setw(2) << (int)(unsigned char)c;
    return ss.str();
}


// Function that initiates file transfer for a given file 
bool beginFileTransfer(char *srcDir,char* fileName,int fileNastiness,C150DgmSocket *sock)
{ 
    // Prepare Packets 
    vector<FilePacket> packets = makeFileTransferPackets(fileName,srcDir,fileNastiness);
    
    // Encode number of packets as content of INITIAL packet
    int totalPackets = packets.size();
    char numPacketsContent[4];
    memcpy(numPacketsContent, &totalPackets, sizeof(totalPackets));

    int numRetries = 0;
    while(numRetries < 5){
    //Send Initial Packet to server ,informing number of packets to expect ,
    // and receive confirmation
    FilePacket initial(INITIAL, 1, fileName, numPacketsContent, sizeof(numPacketsContent));
    *GRADING << "Attempting to send INITIAL packet for file: " << fileName << " with expected packets: " << totalPackets << endl;
    sock -> write((char*)&initial, sizeof(initial));

    //Expect INITIAL_ACK Packet from server with correct filename and num packets
    char initialAckBuffer[sizeof(FilePacket)];
    ssize_t initialAckLength = sock->read(initialAckBuffer, sizeof(initialAckBuffer));
    if (sock->timedout()) {
        *GRADING << "Timeout waiting for INITIAL_ACK packet for file: " << fileName << ", retrying"<<endl;
        beginFileTransfer(srcDir,fileName,fileNastiness,sock);
        numRetries++;
        continue;
       }
    if (initialAckLength != sizeof(initialAckBuffer)) {
      
        numRetries++;
        continue;
      }
    FilePacket initialAck = *(FilePacket*)initialAckBuffer;
    int ackNumPackets;
    memcpy(&ackNumPackets, initialAck.content, sizeof(ackNumPackets));
    if (initialAck.type != INITIAL_ACK || strcmp(initialAck.filename, fileName) != 0 || ackNumPackets != totalPackets) {
        *GRADING << "Received invalid INITIAL_ACK packet for file: " << fileName << ", retrying"<<endl;
        numRetries++;
        continue;
    }
    *GRADING << "Received INITIAL_ACK for file: " << fileName << " with expected packets: " << ackNumPackets << endl;
    break;

     }

    if (numRetries == 5){
        *GRADING << "Failed to receive INITIAL_ACK after 5 attempts for file: " << fileName << ", aborting file transfer"<<endl;
        return false;
    }

    int windowSize = 10;
    int base = 1;
    int lastAck = 0;
 
    
     while (lastAck < totalPackets) {
       sendWindow(sock,packets,base,windowSize);
       sleep(1); // wait for a second to allow for ack packets to arrive
       lastAck = receiveAck(sock,base,windowSize,1);
       base = lastAck + 1;
       if (base > totalPackets) {
           break;
       }
     
     }
        *GRADING << "File transfer complete for file: " << fileName << endl;
        return true;

}

// Send a batch of packets to the server , Sequence numbers from base to base + windowSize
void sendWindow(C150DgmSocket *sock,vector<FilePacket> &packets,int base,int windowSize){
    *GRADING << "Sending window of packets " << base << " to " << base + windowSize - 1 << " for file: " << packets[base - 1].filename << endl;
    int totalPackets = packets.size();
    for (int i = base; i < base + windowSize && i <= totalPackets; i++) {
       
        sock -> write((char*)&packets[i - 1], sizeof(FilePacket));
    }

}

// Receive ACK for packets in the current window
int receiveAck(C150DgmSocket *sock, int base, int windowSize,int attempt) {
    if (attempt > 5) {
        return base - 1; // return last acknowledged packet
    }
    char ackBuffer[sizeof(FilePacket)];

    //Do a read for ACK Packet, ideally expecting ACK for seq no base + windowSize
    ssize_t ackLength = sock->read(ackBuffer, sizeof(ackBuffer));
    if (sock->timedout()) {
        *GRADING << "Timeout waiting for ACK packet, retrying read"<<endl;
        return receiveAck(sock,base,windowSize,attempt + 1);
     }
    if (ackLength != sizeof(ackBuffer)) {
        return receiveAck(sock,base,windowSize,attempt + 1);}

    FilePacket ack = *(FilePacket*)ackBuffer;
    if (ack.type != ACK || (int)ack.sequenceNumber < base || (int)ack.sequenceNumber >= base + windowSize) {
        *GRADING << "Received invalid ACK packet, retrying read"<<endl;
        return receiveAck(sock,base,windowSize,attempt + 1);
    }

    return ack.sequenceNumber;

}
