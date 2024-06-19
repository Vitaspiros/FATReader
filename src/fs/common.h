#ifndef COMMON_FS_H
#define COMMON_FS_H
#include "../extras.h"
#include "fat16.h"
#include "fat32.h"
#include <fstream>
#include <vector>

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
  default:
    return 0;
  }
}

void readFile(FSType fsType, BPB bpb, unsigned int sectorsPerFAT,
              DirectoryEntry entry, std::ifstream &in) {
  const int firstCluster = composeCluster(entry.firstClusterHigh, entry.firstClusterLow);
  const int bytesPerCluster = bpb.sectorsPerCluster * bpb.bytesPerSector;
  unsigned char clusterData[bytesPerCluster + 1];

  const int EOF_VALUE = fsType == FAT32 ? 0x0FFFFFF8 : 0x0000FFF8;
  const int BAD_CLUSTER_VALUE = fsType == FAT32 ? 0x0FFFFFF7 : 0x0000FFF7;

  int currentCluster = firstCluster;

  while (true) {
    in.seekg(getClusterAddress(bpb, sectorsPerFAT, currentCluster));
    in.read((char *)&clusterData, bytesPerCluster);
    clusterData[bytesPerCluster] = '\0';

    std::cout << clusterData;

    int nextCluster = getNextCluster(fsType, bpb, in, currentCluster);
    if (nextCluster >= EOF_VALUE) {
      std::cout << std::endl << "end of file" << std::endl;
      break;
    } else if (nextCluster == BAD_CLUSTER_VALUE) {
      std::cerr << "bad cluster - stopping" << std::endl;
      break;
    }
    currentCluster = nextCluster;
  }
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