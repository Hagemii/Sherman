#ifndef __DIRECTORY_H__
#define __DIRECTORY_H__

#include <thread>

#include <unordered_map>

#include "Common.h"

#include "Connection.h"
#include "GlobalAllocator.h"
#include "Tree.h"

class Directory {
public:
  Directory(DirectoryConnection *dCon, RemoteConnection *remoteInfo,
            uint32_t machineNR, uint16_t dirID, uint16_t nodeID);

  ~Directory();

private:
  DirectoryConnection *dCon;
  RemoteConnection *remoteInfo;
  Tree* treeptr;

  uint32_t machineNR;
  uint16_t dirID;
  uint16_t nodeID;

  std::thread *dirTh;  // 内存池线程，负责分配内存空间、更新root地址，监听RPC请求cq队列

  GlobalAllocator *chunckAlloc;

  void dirThread();

  void sendData2App(const RawMessage *m);

  void process_message(const RawMessage *m);
  
public:
  void setTree(Tree* tree);
};

#endif /* __DIRECTORY_H__ */
