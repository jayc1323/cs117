/* Author : Jay Choudhary
    Fall 2025 CS117 : Distributed Systems FileCopy 
   Purpose : Client program that copies files to the server using UDP . 
  */
//------------------------------------------------------

#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>
#include "c150nastyfile.h"    
#include "endToendPacket.cpp"
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


using namespace std;
// Always use namespace C150NETWORK with COMP 150 IDS framework!
using namespace C150NETWORK;

void checkDirectory(char *dirname);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
void beginEndtoEndCheck(char *srcDir,C150DgmSocket *sock);
string initiate_ReceiveSHA(char *fileName,int attempt, C150DgmSocket *sock); //retry 5 times
int compareSHA_ReceiveConfirmation(string sha,char* srcDir,char *fileName,int attempt,C150DgmSocket *sock); //retry 5 times 

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
      
        C150DgmSocket *sock = new C150NastyDgmSocket(atoi(argv[networkNastinessArg])); 
       
         
        *GRADING << "Client socket created\n";
        // Tell the DGMSocket which server to talk to
        sock -> setServerName(argv[serverArg]); 
        sock -> turnOnTimeouts(1500);

    //-------------------------------------------------------------------------
    // Call endtoend check function
    beginEndtoEndCheck(argv[sourceDirArg],sock);
    *GRADING << "End to end check complete for directory : " << argv[sourceDirArg] << "\n";

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

/* beginEndtoEndCheck(char *srcDir) : 
 Wrapper function for the protocol that
 Receives directory name to check at server end 
 and initiates the end to end check - calling the protocol functions on each file
 in that directory.
   */
void beginEndtoEndCheck(char *srcDir,C150DgmSocket *sock)
{  // Open the source directory
        DIR *SRC = opendir(srcDir);
        if (SRC == NULL) {
            fprintf(stderr,"Error opening source directory %s\n", srcDir);
            exit(8);
        }

        *GRADING << "Starting end to end check for directory: " << srcDir << "\n";

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
             string sha = initiate_ReceiveSHA(fileName,1,sock); //retry 5 times
             int success = compareSHA_ReceiveConfirmation(sha,srcDir,fileName,1,sock); //retry 5 times
                if (success == 0){
                    *GRADING << "File: " << fileName << " end-to-end check succeeded, attempt 1\n";
                } else {
                    *GRADING << "File: " << fileName << " end-to-end check failed, attempt 1\n";
                }
        }
    }
    catch(C150NetworkException& e) { 
        // Write to debug log
        *GRADING << "Caught C150NetworkException: " << e.formattedExplanation() << "\n";
        
        closedir(SRC);
        exit(4);
       
    }
    closedir(SRC);
       return; 
    }



string initiate_ReceiveSHA(char *fileName,int attempt, C150DgmSocket *sock) {
    if (attempt > 5) {
        throw C150NetworkException("Maximum attempts reached for initiate_ReceiveSHA");
    }
   *GRADING << "File: " << fileName << " transmission complete, waiting for end-to-end check, attempt 1";
            
             //STEP 1: Send INITIATE Packet to server , expecting SHA1 for that file in response .
            Packet initialPacket(INITIATE, fileName, "");
          
            
             *GRADING << "Initiating end to end check for file: " << fileName << "\n";
              sock -> write((char*)&initialPacket, sizeof(initialPacket)); 

             
             int readRetry = 0;
             while(readRetry < 5){

              //Expect SHA1 for that file from user
             char sha_buffer[sizeof(Packet)];
             ssize_t readlen = sock->read(sha_buffer, sizeof(sha_buffer));
             
             if(sock->timedout()){
                readRetry++;
                *GRADING << "Timeout waiting for SHA1 from server for file: " << fileName 
                 << ", retrying " << readRetry << "\n"; 
                continue;
             }


                if (readlen == 0) {
                    *GRADING << "Read zero length msg , expecting SHA1\n";
                    throw C150NetworkException("Unexpected length read in client");
                }
               
               Packet sha = *(Packet*)sha_buffer;
               PacketType header = sha.type;
                if (header != SHA || strcmp(sha.filename, fileName) != 0) {
                     *GRADING << "Received non-SHA packet, expecting SHA1 ,retrying read";
                        readRetry++;
                        continue;
                }

               string sha1content = string(sha.content);
               *GRADING << "Received SHA for file: " << fileName;
               return sha1content;
            }

               // maybe 1st initiate packet wasn't sent , retry
               return initiate_ReceiveSHA(fileName,attempt+1,sock); //retry 5 times


}

int compareSHA_ReceiveConfirmation(string sha,char* srcDir,char *fileName,int attempt,C150DgmSocket *sock){
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
            string computed_sha = string((char*)obuf);
            int result = computed_sha.compare(sha);
             if (result == 0){
                status = "SUCCESS";
                  printf("SHA Check succeeded for file: %s\n", fileName);
                  *GRADING << "SHA Check succeeded for file: " << fileName;
             }else{
                status = "FAILURE";
                    printf("SHA Check failed for file: %s\n", fileName);
                    *GRADING << "SHA Check failed for file: " << fileName;
             }
             	delete t;
	            delete buffer;
           
             
                //Send STATUS Packet
             Packet statusPacket(SHA, fileName, status.c_str());
             *GRADING << "Sending SHA check result to server for file: " << fileName <<" with result: " << status;
           
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
                    << ", retrying " << attempt << "\n"; 
                    numRetries++;
                    continue;
                }

                // CASE : Read length 0 , throw exception 
             if (confirmationLength != sizeof(confirmationPacket)) {
                
                *GRADING<<"Read failed for confirmation , throwing exception\n";
                throw C150NetworkException("Unexpected length read in client");
            }



                Packet confirm = *(Packet*)confirmationPacket;

                // Received Packet but invalid type , discard and retry
                if (confirm.type!=CONFIRM || strcmp(confirm.filename, fileName) != 0) {
                        *GRADING << "Received non-CONFIRM packet, expecting CONFIRM ,retrying read";
                        numRetries++;
                        continue;
                 }

                else {
                    
                    // END TO END CHECKING COMPLETE FOR THAT FILE
                    *GRADING << "Received confirmation from server for file: " << fileName;
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


