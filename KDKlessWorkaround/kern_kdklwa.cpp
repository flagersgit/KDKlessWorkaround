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
#include <Headers/kern_devinfo.hpp>
#include <Headers/plugin_start.hpp>
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
    KernelPatcher::RouteRequest request(ioGA2StartSymbol, wrapIOGA2Start, orgIOGA2Start);
    if (!patcher.routeMultiple(index, &request, 1, address, size)) {
      SYSLOG("KDKLWA", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
      patcher.clearError();
    }
  }
}

/* static */
bool KDKLWA::wrapIOGA2Start(IOService *that, IOService *provider) {
  if (!verifyPluginOnDisk(that)) {
    return false;
  }
  
  return FunctionCast(wrapIOGA2Start, callbackKDKLWA->orgIOGA2Start)(that, provider);
}

/* static */
bool KDKLWA::verifyPluginOnDisk(IOService *ioGA2) {
  bool rval = true;
  bool isAMD = false;
  OSString *pluginProperty;
  OSDictionary *amdLookupDict;
  char pathbuf[PATH_MAX];
  
  memset(&pathbuf, 0, PATH_MAX);
  strcpy((char *)&pathbuf, "/System/Library/Extensions/", PATH_MAX);
  
  const char * thatClassName = ioGA2->getMetaClass()->getClassName();
  if (strstr(thatClassName, "AMDRadeonX") && !BaseDeviceInfo::get().cpuHasAvx2 && getKernelVersion() >= KernelVersion::Ventura) {
    DBGLOG(MODULE_SHORT, "Found AMD subclass of IOGraphicsAccelerator2 (%s); using lookup dict", thatClassName);
    isAMD = true;
    amdLookupDict = OSDynamicCast(OSDictionary, ADDPR(selfInstance)->getProperty("AMDClassToPluginMap"));
    if (!amdLookupDict)
      return false;
    pluginProperty = OSDynamicCast(OSString, amdLookupDict->getObject(thatClassName));
  } else {
    pluginProperty = OSDynamicCast(OSString, ioGA2->getProperty("MetalPluginName"));
  }
  
  if (!pluginProperty)
    return false;
  
  strcat((char *)&pathbuf, pluginProperty->getCStringNoCopy());
  if (isAMD) {
    strcat((char *)&pathbuf, ".bundle/Contents/MacOS/");
    strcat((char *)&pathbuf, pluginProperty->getCStringNoCopy());
    rval = nodeExistsAtPath((char *)&pathbuf, VREG);
  } else {
    strcat((char *)&pathbuf, ".bundle");
    rval = nodeExistsAtPath((char *)&pathbuf, VDIR);
  }
  
  // Maybe check for OpenGL here too?
  
  return rval;
}

/* static */
bool KDKLWA::nodeExistsAtPath(const char *path, vtype type) {
  bool rval = true;
  vnode_t vnode = NULLVP;
  vfs_context_t ctxt = vfs_context_create(nullptr);

  errno_t err = vnode_lookup(path, 0, &vnode, ctxt);
  if (err) {
    SYSLOG(MODULE_SHORT, "failed to find node at path: %s", path);
    rval = false;
  } else {
    if (vnode_vtype(vnode) != type)
      rval = false;
    vnode_put(vnode);
  }
  
  vfs_context_rele(ctxt);
  return rval;
}
