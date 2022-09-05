//
//  kern_start.cpp
//  KDKlessWorkaround
//
//  Copyright © 2022 flagers. All rights reserved.
//

// Lilu headers
#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_kdklwa.hpp"

static KDKLWA kdklwa;

#pragma mark - Plugin start
static void pluginStart() {
    DBGLOG(MODULE_SHORT, "start");
    kdklwa.init();
};

// Boot args.
static const char *bootargOff[] {
    "-kdklwaoff"
};
static const char *bootargDebug[] {
    "-kdklwadbg"
};
static const char *bootargBeta[] {
    "-kdklwabeta"
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::BigSur,
    KernelVersion::Monterey,
    pluginStart
};
