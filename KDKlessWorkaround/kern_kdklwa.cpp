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
  const char * thatClassName = that->getMetaClass()->getClassName();
  if (strstr(thatClassName, "AMDRadeonX")) {
    DBGLOG(MODULE_SHORT, "Found AMD subclass of IOGraphicsAccelerator2; interposing setProperty in %s's vtable.", thatClassName);
    KernelPatcher::routeVirtual(that, 0x4B, wrapIORegEntrySetProperty, &callbackKDKLWA->orgIORegEntrySetProperty);
  } else if (!verifyPluginOnDisk(that)) {
    return false;
  }
  
  return FunctionCast(wrapIOGA2Start, callbackKDKLWA->orgIOGA2Start)(that, provider);
}

// Break AMD IOGraphicsAccelerator2 on AVX2-less systems to prevent crashes before system is patched.
// AMD sets the MetalPluginName property in the binary rather than in the Info.plist.
/* static */
bool KDKLWA::wrapIORegEntrySetProperty(IORegistryEntry *that, const OSSymbol *aKey, OSObject *anObject) {
  char pathbuf[PATH_MAX];
  
  DBGLOG(MODULE_SHORT, "setProperty attempted on %s", that->getMetaClass()->getClassName());
  if (aKey->isEqualTo("MetalPluginName")) {
    if (!BaseDeviceInfo::get().cpuHasAvx2 && getKernelVersion() >= KernelVersion::Ventura) {
      OSString *pluginName = OSDynamicCast(OSString, anObject);
      if (pluginName) {
        memset(&pathbuf, 0, PATH_MAX);
        strcpy((char *)&pathbuf, "/System/Library/Extensions/", PATH_MAX);
        strcat((char *)&pathbuf, pluginName->getCStringNoCopy());
        strcat((char *)&pathbuf, ".bundle/Contents/MacOS/");
        strcat((char *)&pathbuf, pluginName->getCStringNoCopy());
        if (!nodeExistsAtPath((char *)&pathbuf, VREG)) {
          SYSLOG(MODULE_SHORT, "Metal plugin for %s not present on disk, terminating service.", that->getMetaClass()->getClassName());
          OSDynamicCast(IOService, that)->terminate(kIOServiceRequired);
        }
      }
    }
  }
  return callbackKDKLWA->orgIORegEntrySetProperty(that, aKey, anObject);
}

/* static */
bool KDKLWA::verifyPluginOnDisk(IOService *ioGA2) {
  bool rval = true;
  OSString *pluginProperty;
  char pathbuf[PATH_MAX];
  
  memset(&pathbuf, 0, PATH_MAX);
  strcpy((char *)&pathbuf, "/System/Library/Extensions/", PATH_MAX);
  
  pluginProperty = OSDynamicCast(OSString, ioGA2->getProperty("MetalPluginName"));
  if (!pluginProperty)
    return false;
  strcat((char *)&pathbuf, pluginProperty->getCStringNoCopy());
  strcat((char *)&pathbuf, ".bundle");
  
  rval = nodeExistsAtPath((char *)&pathbuf, VDIR);
  
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
