#ifndef COMMON_FS_H
#define COMMON_FS_H
#include "../extras.h"
#include "fat12.h"
#include "fat16.h"
#include "fat32.h"
#include <fstream>
#include <ios>
#include <vector>

// we set the End Of Cluster Chain value for each filesystem
const int EOCC32 = 0x0FFFFFF8;
const int EOCC16 = 0x0000FFF8;
const int EOCC12 = 0x00000FF8;

// we set the Bad Cluster value for each filesystem
const int BAD_CLUSTER32 = 0x0FFFFFF7;
const int BAD_CLUSTER16 = 0x0000FFF7;
const int BAD_CLUSTER12 = 0x00000FF7;

void readBPB(BPB *bpb, std::ifstream &in) {
  in.read((char *)bpb->jmp, 3);
  in.read((char *)bpb->oem, 8);
  bpb->oem[8] = '\0';
  read(&bpb->bytesPerSector, in);
  read(&bpb->sectorsPerCluster, in);
  read(&bpb->reservedSectors, in);
  read(&bpb->FATs, in);
  read(&bpb->rootDirectoryEntries, in);
  read(&bpb->sectorsCount, in);
  read(&bpb->mediaDescriptorType, in);
  read(&bpb->sectorsPerFAT, in); // Number of sectors per FAT - This is only for FAT12 and FAT16
  read(&bpb->sectorsPerTrack, in);
  read(&bpb->headsCount, in);
  read(&bpb->hiddenSectors, in);
  read(&bpb->sectorsCount_large, in);
}

int getNextCluster(FSType fsType, BPB bpb, std::ifstream &in, int cluster) {
  switch (fsType) {
  case FAT32:
    return fat32::getNextCluster(in, bpb, cluster);
  case FAT16:
    return fat16::getNextCluster(in, bpb, cluster);
  case FAT12:
    return fat12::getNextCluster(in, bpb, cluster);
  default:
    return 0;
  }
}

void readFile(FSType fsType, BPB bpb, unsigned int sectorsPerFAT,
              DirectoryEntry entry, std::ifstream &in) {
  const int firstCluster = composeCluster(entry.firstClusterHigh, entry.firstClusterLow);
  const int bytesPerCluster = bpb.sectorsPerCluster * bpb.bytesPerSector;
  unsigned char clusterData[bytesPerCluster + 1];

  // get the appropriate values for each filesystem
  const int EOCC_VALUE = fsType == FAT32 ? EOCC32 : (fsType == FAT16 ? EOCC16 : EOCC12);
  const int BAD_CLUSTER_VALUE = fsType == FAT32 ? BAD_CLUSTER32 : (fsType == FAT16 ? BAD_CLUSTER16 : BAD_CLUSTER12);

  int currentCluster = firstCluster;

  while (true) {
    in.seekg(getClusterAddress(bpb, sectorsPerFAT, currentCluster));
    in.read((char *)&clusterData, bytesPerCluster);
    clusterData[bytesPerCluster] = '\0';

    std::cout << clusterData;

    int nextCluster = getNextCluster(fsType, bpb, in, currentCluster);
    if (nextCluster >= EOCC_VALUE) {
      std::cout << std::endl << "end of file" << std::endl;
      break;
    } else if (nextCluster == BAD_CLUSTER_VALUE) {
      std::cerr << "bad cluster - stopping" << std::endl;
      break;
    }
    currentCluster = nextCluster;
  }
}

// This function isn't in any namespace because it will be used by both FAT16 and FAT12
void readDirectory(FSType fsType, std::ifstream& in, BPB bpb, int cluster, std::vector<DirectoryEntry>& entries) {
  // Only for FAT16/12. FAT32 has its own function in its own namespace
  if (fsType == FAT32) return;

  const int EOCC_VALUE = fsType == FAT16 ? EOCC16 : EOCC12;
  const int BAD_CLUSTER_VALUE = fsType == FAT16 ? BAD_CLUSTER16 : BAD_CLUSTER12;

  int position;
  if (cluster == -1) {
    const int firstFATSector = bpb.reservedSectors;
    const int FATSize = bpb.sectorsPerFAT * bpb.FATs * bpb.bytesPerSector; // FAT size in bytes
    // The root directory on FAT12/16 is immediately after the FAT
    position = firstFATSector * bpb.bytesPerSector + FATSize;
  } else {
    position = getClusterAddress(bpb, bpb.sectorsPerFAT, cluster);
  }

  in.seekg(position);
  while (true) {
    bool hasMore = readDirectoryEntry(in, entries);
    if (!hasMore) break;

    if ((int)in.tellg() - position >= bpb.sectorsPerCluster * bpb.bytesPerSector) {
      // if we read more than one cluster go to the next
      int nextCluster = getNextCluster(fsType, bpb, in, cluster);
      if (nextCluster == EOCC_VALUE) return; // end of cluster chain
      else if (nextCluster == BAD_CLUSTER_VALUE) {
        // bad cluster
        std::cerr << "bad cluster while reading directory" << std::endl;
        return;
      }

      position = getClusterAddress(bpb, bpb.sectorsPerFAT, cluster);
      in.seekg(position);
    }
  }
}

// Common function for reading the EBPB on FAT16 and FAT12
// FAT32 has its own function for that because the EBPB is different
void readEBPB(EBPB* ebpb, std::ifstream& in) {
  read(&ebpb->driveNumber, in);
  in.ignore(); // Reserved
  read(&ebpb->signature, in);
  read(&ebpb->volumeId, in);
  in.read((char*)&ebpb->volumeLabel, 11);
  ebpb->volumeLabel[11] = '\0';
  in.read((char*)ebpb->systemId, 8);
  ebpb->systemId[8] = '\0';
  // I ain't reading allat
  in.ignore(448); // Boot code
  in.ignore(2); // Bootable partition signature
}

FSType detectFSType(BPB bpb) {
  int totalSectors =
      (bpb.sectorsCount == 0) ? bpb.sectorsCount_large : bpb.sectorsCount;
  int sectorsPerFAT = bpb.sectorsPerFAT;
  if (sectorsPerFAT == 0)
    return FAT32; // Only FAT16 and FAT12 have this value set
  int rootDirectorySize =
      ((bpb.rootDirectoryEntries * 32) + (bpb.bytesPerSector - 1)) /
      bpb.bytesPerSector; // Root directory size in sectors

  int dataSectors =
      totalSectors -
      (bpb.reservedSectors + (bpb.FATs * sectorsPerFAT) + rootDirectorySize);
  int totalClusters = dataSectors / bpb.sectorsPerCluster;

  if (totalClusters < 4085)
    return FAT12;
  if (totalClusters < 65525)
    return FAT16;
  else
    return FAT32;
}

#endif