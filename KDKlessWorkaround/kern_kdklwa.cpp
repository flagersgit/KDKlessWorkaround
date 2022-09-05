//
//  kern_kdklwa.cpp
//  KDKlessWorkaround
//
//  Created by flagers on 9/5/22.
//  Copyright © 2022 flagers. All rights reserved.
//

#include <sys/vnode.h>
#include <sys/syslimits.h>
#include <Headers/kern_api.hpp>
#include "kern_kdklwa.hpp"

static KDKLWA *callbackKDKLWA = nullptr;

void KDKLWA::init() {
  callbackKDKLWA = this;
  
  lilu.onKextLoadForce(kextList, arrsize(kextList),
  [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    callbackKDKLWA->processKext(patcher, index, address, size);
  }, this);
}

void KDKLWA::deinit() {
    
}

void KDKLWA::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
  if (index == kextList[0].loadIndex) {
    KernelPatcher::RouteRequest request(ioGA2StartSymbol, ioGA2StartHandler, orgIOGA2Start);
    if (!patcher.routeMultiple(index, &request, 1, address, size)) {
      SYSLOG("KDKLWA", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
      patcher.clearError();
    }
  }
}

/* static */
bool KDKLWA::ioGA2StartHandler(IOService *that, IOService *provider) {
  if (!verifyPluginsOnDisk(that))
    return false;
  
  return FunctionCast(ioGA2StartHandler, callbackKDKLWA->orgIOGA2Start)(that, provider);
}

static const char *basePathSLE[] { "/System/Library/Extensions/" };

/* static */
bool KDKLWA::verifyPluginsOnDisk(IOService *ioGA2) {
  bool rval = true;
  OSString *pluginProperty;
  char pathbuf[PATH_MAX];
  
  memset(&pathbuf, 0, sizeof(pathbuf));
  strcat((char *)&pathbuf, (char *)basePathSLE);
  
  pluginProperty = OSDynamicCast(OSString, ioGA2->getProperty("MetalPluginName"));
  if (!pluginProperty)
    return false;
  strcat((char *)&pathbuf, pluginProperty->getCStringNoCopy());
  
  rval = dirExistsAtPath((char *)&pathbuf);
  
  // Maybe check for OpenGL here too?
  
  return rval;
}

/* static */
bool KDKLWA::dirExistsAtPath(const char *path) {
  bool rval = true;
  vnode_t vnode = NULLVP;
  vfs_context_t ctxt = vfs_context_create(nullptr);

  errno_t err = vnode_lookup(path, 0, &vnode, ctxt);
  if (err) {
    SYSLOG(MODULE_SHORT, "failed to find directory at path: %s", path);
    rval = false;
  } else {
    if (vnode_vtype(vnode) != VDIR)
      rval = false;
  }
  
  vfs_context_rele(ctxt);
  return rval;
}
