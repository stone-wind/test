/*
* post-receive.cc
* CodeCloud Git Hook
* hook file: post-receive
* author: Force.Charlie
* Date: 2016.06
* Copyright (C) 2017. OSChina.NET. All Rights Reserved.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <path.hpp>
#include <refs.hpp>
#include "profile.hpp"
#include "sync.hpp"
#include "resque.hpp"
#include "debug.hpp"

#define DEFAULT_REMOTE "git://"

int SynchronizeTaskRun(const char *pwn, const char *keyid,
                       const Paramters &paramters) {
  SynchronizeTask syncTask;
  if (!syncTask.InitializeTask(paramters.synclog)) {
    DebugMessage("InitializeTask Error %s", strerror(errno));
    return 2;
  }
  if (!syncTask.OfflineExecute(pwn, keyid, paramters.slaves)) {
    DebugMessage("OfflineExecute Error %s", strerror(errno));
    return 1;
  }
  return 0;
}

bool ResqueActive(const char *pwn, const char *keyid,
                  const Paramters &paramters) {

  Resque resque;
  if (!resque.Initialize(paramters.redis, paramters.redisPort)) {
    DebugMessage("Resque Initialize Redis Connection Failed !");
    return false;
  }
  std::vector<ChangedRef> refs;
  if (!DiscoverChangedRefs(refs)) {
    DebugMessage("DiscoverChangedRefs Failed !");
    return false;
  }
  for (auto &r : refs) {
    if (!resque.Insert(pwn, r.refname.c_str(), r.oldrev.c_str(),
                       r.newrev.c_str(), keyid)) {
#ifdef DEBUG
      auto s = r.dump();
      DebugMessage("Resque Failed: %s %s \n%s\n", pwn, keyid, s.c_str());
#endif
    }
  }
  return true;
}

int main(int argc, char **argv) {
  //// Feature
  (void)argc;
  (void)argv;
  auto keyid = getenv(KEYID);
  if (keyid == nullptr) {
    /// Not running
    return 0;
  }
  if (keyid && strncmp(keyid, "sync-", sizeof("sync-") - 1) == 0) {
    /// We skip when keyid start with sync-, because, this git-sync-daemon set
    return 0;
  }
  std::string pwn; /// path with namespace
  char buf[PATH_MAX];
  auto repodir = getcwd(buf, PATH_MAX);
  if (!PathWithNamespace(repodir, pwn)) {
    DebugMessage("Worng DIR: %s\n", repodir);
    return 1;
  }
  Paramters paramters;
  if (!paramters.Parseconfig()) {
    DebugMessage("Parseconfig failed !\n");
  }
  //// check keyid must user- or key- prefix, other service not call redis
  if (strncmp(keyid, "user-", sizeof("user-") - 1) == 0 ||
      strncmp(keyid, "key-", sizeof("key-") - 1) == 0) {
    if (!ResqueActive(pwn.c_str(), keyid, paramters)) {
      //// push redis
      DebugMessage("ResqueActive Error!\n");
      return 1;
    }
  }
  if (paramters.enableSync) {
    SynchronizeTaskRun(pwn.c_str(), keyid, paramters);
  }
  return 0;
}

