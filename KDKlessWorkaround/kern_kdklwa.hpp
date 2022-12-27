//
//  kern_kdklwa.hpp
//  KDKlessWorkaround
//
//  Created by flagers on 9/5/22.
//  Copyright © 2022 flagers. All rights reserved.
//

#ifndef kern_kdklwa_hpp
#define kern_kdklwa_hpp

#define MODULE_SHORT "kdklwa"

#include <IOKit/IOService.h>
#include <Headers/kern_patcher.hpp>

static const char *kextIOAccel2[] { "/System/Library/Extensions/IOAcceleratorFamily2.kext/Contents/MacOS/IOAcceleratorFamily2" };
static KernelPatcher::KextInfo kextList[] {
  {"com.apple.iokit.IOAcceleratorFamily2", kextIOAccel2, arrsize(kextIOAccel2), {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

static const char *ioGA2StartSymbol { "__ZN22IOGraphicsAccelerator25startEP9IOService" };

class KDKLWA {
public:
  void init();
  void deinit();
    
private:
  void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t addres, size_t size);
    
  // argument 'that' is actually an IOGraphicsAccelerator2 sub-class,
  // but we don't care about those interfaces, so just use IOService.
  static bool wrapIOGA2Start(IOService *that, IOService *provider);
  mach_vm_address_t orgIOGA2Start {0};
  
  static bool wrapIORegEntrySetProperty(IORegistryEntry *that, const OSSymbol *aKey, OSObject *anObject);
  using t_IORegEntrySetProperty = bool (*)(IORegistryEntry *that, const OSSymbol *aKey, OSObject *anObject);
  t_IORegEntrySetProperty orgIORegEntrySetProperty {nullptr};
    
  static bool verifyPluginOnDisk(IOService *ioGA2);
  
  static bool nodeExistsAtPath(const char *path, vtype type);
};


#endif /* kern_kdklwa_hpp */
