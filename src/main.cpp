#include <fstream>
#include <ios>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>

#include "extras.h"
#include "fs/common.h"

std::vector<DirectoryEntry> currentDirEntries;

int main(int argc, const char** argv) {
    BPB bpb;
    EBPB_32 ebpb;
    FSInfo fsInfo;

    FSType fsType = FAT32;

    // check argument count
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <input drive>" << std::endl;
        return -1;
    }

    // open the file
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "could not open drive!" << std::endl;
        return -1; 
    } else
        std::cout << "Drive opened." << std::endl;

    readBPB(&bpb, in);
    fat32::readEBPB(&ebpb, in);
    fat32::readFSInfo(bpb, ebpb, &fsInfo, in);
    
    readDirectory(fsType, bpb, ebpb.sectorsPerFAT, in, ebpb.rootDirCluster, currentDirEntries);
    while (true) {
        std::cout << std::endl << "> ";
        
        std::string command;
        std::getline(std::cin, command, '\n');

        if (command == "fsinfo") {
            // Print the information
            std::cout << "**** Info for BPB (BIOS Parameter Block) ****" << std::endl << std::endl;
            printBPBInfo(bpb);

            std::cout << std::endl << "**** Info for EBPB (Extended BIOS Parameter Block) ****" << std::endl;
            printEBPBInfo(ebpb);
            std::cout << std::endl << "**** Info for FSInfo structure (Filesystem info) ****" << std::endl;
            printFSInfo(fsInfo);
        } else if (command == "fileinfo") {
            std::string filename;

            std::cout << "Filename: ";
            std::getline(std::cin, filename, '\n');

            bool found = false;
            for (int i = 0; i < currentDirEntries.size(); i++) {
                bool hasLongFilename = !currentDirEntries.at(i).longFilename.empty();
                std::string entryFilename;
                if (hasLongFilename)
                    entryFilename = currentDirEntries.at(i).longFilename;
                else
                    entryFilename = std::string((char*)(currentDirEntries.at(i).filename));
                rtrim(entryFilename);
                if (entryFilename == filename) {
                    printDirectoryEntryInfo(currentDirEntries.at(i));
                    found = true;
                    break;
                }
            }

            if (!found) std::cout << "File " << filename << "was not found." << std::endl;
        } else if (command == "ls") {
            for (int i = 0; i < currentDirEntries.size(); i++) {
                printDirectoryEntry(currentDirEntries.at(i));
            }
        } else if (command == "exit" || command == "quit") {
            break;
        } else {
            bool found = false;
            for (int i = 0; i < currentDirEntries.size(); i++) {
                bool hasLongFilename = !currentDirEntries.at(i).longFilename.empty();
                std::string entryFilename;
                if (hasLongFilename)
                    entryFilename = currentDirEntries.at(i).longFilename;
                else
                    entryFilename = std::string((char*)(currentDirEntries.at(i).filename));
                rtrim(entryFilename);
                if (entryFilename == command) {
                    bool isDirectory = (currentDirEntries.at(i).attributes & 0x10) != 0;
                    if (!isDirectory)
                        readFile(fsType, bpb, ebpb.sectorsPerFAT, currentDirEntries.at(i), in);
                    else {
                        const int firstCluster = (currentDirEntries.at(i).firstClusterHigh << 16) | currentDirEntries.at(i).firstClusterLow;
                        readDirectory(fsType, bpb, ebpb.sectorsPerFAT, in, firstCluster, currentDirEntries);
                        std::cout << "Switched to directory " << entryFilename << std::endl;
                    }
                    found = true;
                }
            }
            if (!found) std::cout << "Unknown command." << std::endl;
        }
    }

    in.close();
    return 0;
}