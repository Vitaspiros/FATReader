#include <fstream>
#include <ios>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>

#include "extras.h"
#include "fs/common.h"
#include "fs/fat16.h"

std::vector<DirectoryEntry> currentDirEntries;

int main(int argc, const char** argv) {
    BPB bpb;
    EBPB_32 ebpb_32;
    EBPB ebpb;
    FSInfo fsInfo;

    FSType fsType;

    int sectorsPerFAT = 0;

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
    if (bpb.jmp[0] == 0xEB && bpb.jmp[2] == 0x90) {
        std::cout << "FAT image detected (by JMP signature)" << std::endl;
    } else {
        std::cerr << "Image does not have the correct JMP signature." << std::endl;
        std::cerr << "Probably not a valid FAT image. Exiting." << std::endl;
        in.close();
        return -1;
    }

    fsType = detectFSType(bpb);
    if (fsType == FAT32) {
        std::cout << "Filesystem detected as FAT32" << std::endl;

        fat32::readEBPB(&ebpb_32, in);
        sectorsPerFAT = ebpb_32.sectorsPerFAT;
        fat32::readFSInfo(bpb, ebpb_32, &fsInfo, in);

        // Read root directory
        fat32::readDirectory(in, bpb, ebpb_32, ebpb_32.rootDirCluster, currentDirEntries);
    } else {
        sectorsPerFAT = bpb.sectorsPerFAT;
        if (fsType == FAT16) {
            std::cout << "Filesystem detected as FAT16" << std::endl;
        } else if (fsType == FAT12)
            std::cout << "Filesystem detected as FAT12" << std::endl;

        // These steps are common for both FAT16 and FAT12

        readEBPB(&ebpb, in);
        // Read root directory (cluster -1 for the root directory)
        readDirectory(fsType, in, bpb, -1, currentDirEntries);
    }
    while (true) {
        std::cout << std::endl << "> ";
        
        std::string command;
        std::getline(std::cin, command, '\n');

        if (command == "fsinfo") {
            // Print the information
            std::cout << "**** Info for BPB (BIOS Parameter Block) ****" << std::endl << std::endl;
            printBPBInfo(bpb);

            std::cout << std::endl << "**** Info for EBPB (Extended BIOS Parameter Block) ****" << std::endl;
            if (fsType == FAT32) printEBPB32Info(ebpb_32);
            else printEBPBInfo(ebpb); // FAT16 and FAT12 have common EBPB

            if (fsType == FAT32) {
                std::cout << std::endl << "**** Info for FSInfo structure (Filesystem info) ****" << std::endl;
                printFSInfo(fsInfo);
            }
        } else if (command == "fileinfo") {
            std::string filename;

            std::cout << "Filename: ";
            std::getline(std::cin, filename, '\n');

            bool found = false;
            for (int i = 0; i < currentDirEntries.size(); i++) {
                DirectoryEntry entry = currentDirEntries.at(i);
                bool hasLongFilename = !entry.longFilename.empty();
                std::string entryFilename;
                if (hasLongFilename)
                    entryFilename = entry.longFilename;
                else
                    entryFilename = std::string((char*)(entry.filename));
                rtrim(entryFilename);
                if (entryFilename == filename) {
                    printDirectoryEntryInfo(entry);
                    found = true;
                    break;
                }
            }

            if (!found) std::cout << "File " << filename << " was not found." << std::endl;
        } else if (command == "ls") {
            for (int i = 0; i < currentDirEntries.size(); i++) {
                printDirectoryEntry(currentDirEntries.at(i));
            }
        } else if (command == "exit" || command == "quit") {
            break;
        } else {
            bool found = false;
            for (int i = 0; i < currentDirEntries.size(); i++) {
                DirectoryEntry entry = currentDirEntries.at(i);
                bool hasLongFilename = !entry.longFilename.empty();
                std::string entryFilename;
                if (hasLongFilename)
                    entryFilename = entry.longFilename;
                else
                    entryFilename = std::string((char*)(entry.filename));
                rtrim(entryFilename);
                if (entryFilename == command) {
                    bool isDirectory = (entry.attributes & 0x10) != 0;
                    if (!isDirectory)
                        readFile(fsType, bpb, sectorsPerFAT, entry, in);
                    else {
                        const int firstCluster = composeCluster(entry.firstClusterHigh, entry.firstClusterLow);
                        currentDirEntries.clear();
                        if (fsType == FAT32) fat32::readDirectory(in, bpb, ebpb_32, firstCluster, currentDirEntries);
                        else if (fsType == FAT16) readDirectory(FAT16, in, bpb, firstCluster, currentDirEntries);
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