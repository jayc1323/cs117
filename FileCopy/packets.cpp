#include "packets.h"

using namespace std;
// Always use namespace C150NETWORK with COMP 150 IDS framework!
using namespace C150NETWORK;

string
makeFileName(string dir, string name) {
  stringstream ss;

  ss << dir;
  // make sure dir name ends in /
  if (dir.substr(dir.length()-1,1) != "/")
    ss << '/';
  ss << name;     // append file name to dir
  return ss.str();  // return dir/name
  
}

bool isFile(string fname) {
  const char *filename = fname.c_str();
  struct stat statbuf;  
  if (lstat(filename, &statbuf) != 0) {
    fprintf(stderr,"isFile: Error stating supplied source file %s\n", filename);
    return false;
  }

  if (!S_ISREG(statbuf.st_mode)) {
    fprintf(stderr,"isFile: %s exists but is not a regular file\n", filename);
    return false;
  }
  return true;
}

// ------------------------------------------------------
//
//                   checkDirectory
//
//  Make sure directory exists
//     
// ------------------------------------------------------

void
checkDirectory(char *dirname) {
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

vector<FilePacket> makeFileTransferPackets(string fileName,string dir,int nastiness){
    vector<FilePacket> packets;
    void *fopenretval;
    size_t len;
    string errorString;
    char *buffer;
    struct stat statbuf;  
    size_t sourceSize;
    string sourceName = makeFileName(dir, fileName);
    //
    // make sure the file we're copying is not a directory
    // 
    if (!isFile(sourceName)) {
        cerr << "Input file " << sourceName << " is a directory or other non-regular file. Skipping" << endl;
        exit(1);
    }

    try {

    //
    // Read whole input file 
    //
    if (lstat(sourceName.c_str(), &statbuf) != 0) {
      fprintf(stderr,"copyFile: Error stating supplied source file %s\n", sourceName.c_str());
     exit(20);
    }
  
    //
    // Make an input buffer large enough for
    // the whole file
    //
    sourceSize = statbuf.st_size;
    buffer = (char *)malloc(sourceSize);

    //
    // Define the wrapped file descriptors
    //
    // All the operations on outputFile are the same
    // ones you get documented by doing "man 3 fread", etc.
    // except that the file descriptor arguments must
    // be left off.
    //
    // Note: the NASTYFILE type is meant to be similar
    //       to the Unix FILE type
    //
    NASTYFILE inputFile(nastiness);
    // do an fopen on the input file
    fopenretval = inputFile.fopen(sourceName.c_str(), "rb");  
                                         // wraps Unix fopen
                                         // Note rb gives "read, binary"
                                         // which avoids line end munging
  
    if (fopenretval == NULL) {
      cerr << "Error opening input file " << sourceName << 
	      " errno=" << strerror(errno) << endl;
      exit(12);
    }
     // 
    // Read the whole file
    //
    len = inputFile.fread(buffer, 1, sourceSize);
    if (len != sourceSize) {
      cerr << "Error reading file " << sourceName << 
	      "  errno=" << strerror(errno) << endl;
      exit(16);
    }
  
    if (inputFile.fclose() != 0 ) {
      cerr << "Error closing input file " <<
	      " errno=" << strerror(errno) << endl;
      exit(16);
    }
    
    int size = (int)sourceSize;
    int numPackets = (size / 450) + ((size % 450) ? 1 : 0);
    for (int i = 0; i < numPackets; i++){
        int start = i * 450;
        int end = ((i + 1) * 450 < size) ? (i + 1) * 450 : size;

        FilePacketType type = DATA;
        packets.emplace_back(type, (uint32_t)(i + 1), fileName.c_str(), buffer + start, end - start);
        }
   }
   catch (C150Exception& e) {
       cerr << "nastyfiletest:copyfile(): Caught C150Exception: " << 
	       e.formattedExplanation() << endl;
    
  }
  free(buffer);
    return packets;



}

/* int main(){

    Packet p(INITIATE, "example.txt", "examplecontent");
    char buffer[sizeof(Packet)];
    printf("Type: %d\n", p.type);
     printf("Filename: %s\n", p.filename);
     printf("Content: %s\n", p.content); 
    string dir = "SRC";
    string file = "romeo.txt";
    vector<FilePacket> packets = makeFileTransferPackets(file,dir,0);
    for(auto &pkt : packets){
        printf("Type: %d, SeqNum: %u, Filename: %s, ContentLength: %u\n", pkt.type, pkt.sequenceNumber, pkt.filename, pkt.contentLength);
    }
    return 0;
 } 
 */ 
