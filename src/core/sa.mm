#import <ScriptingBridge/ScriptingBridge.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

static inline bool IsRoot(void)
{
    return geteuid() == 0 || getuid() == 0;
}

static bool
CreateDirectoryStruct(void)
{
    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax", 0755)) {
        goto err;
    }

    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents", 0755)) {
        goto err;
    }

    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/MacOS", 0755)) {
        goto err;
    }

    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources", 0755)) {
        goto err;
    }

    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle", 0755)) {
        goto err;
    }

    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle/Contents", 0755)) {
        goto err;
    }

    if (mkdir("/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle/Contents/MacOS", 0755)) {
        goto err;
    }

    return true;

err:
    return false;
}

static bool
WriteTextFile(const char *Buffer, unsigned int BufferSize, const char *OutputFile)
{
    FILE *Handle = fopen(OutputFile, "w");
    if (!Handle) return false;

    size_t Written = fwrite(Buffer, BufferSize, 1, Handle);
    bool Result = Written == 1;
    fclose(Handle);

    return Result;
}

static bool
WriteBinaryFile(const unsigned char *Buffer, unsigned int BufferSize, const char *OutputFile)
{
    FILE *Handle = fopen(OutputFile, "wb");
    if (!Handle) return false;

    size_t Written = fwrite(Buffer, BufferSize, 1, Handle);
    bool Result = Written == 1;
    fclose(Handle);

    return Result;
}

static void PrepBinaries(void)
{
    // NOTE(koekeishiya): We just call chmod and codesign using system for now..
    system("chmod +x \"/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/MacOS/CHWMInjector\"");
    system("chmod +x \"/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle/Contents/MacOS/chunkwm-sa\"");
    system("codesign -f -s - \"/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/MacOS/CHWMInjector\" 2>/dev/null");
    system("codesign -f -s - \"/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle/Contents/MacOS/chunkwm-sa\" 2>/dev/null");
}

static bool RemoveSA(void)
{
    int code = system("rm -rf /System/Library/ScriptingAdditions/CHWMInjector.osax 2>/dev/null");
    if (code == -1)  return false;
    if (code == 127) return false;
    return code == 0;
}

static bool IsSAInstalled(void)
{
    DIR *dir = opendir("/System/Library/ScriptingAdditions/CHWMInjector.osax");
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

static inline bool IsMacOSCatalinaOrNewer(void)
{
    NSOperatingSystemVersion os_version = [[NSProcessInfo processInfo] operatingSystemVersion];
    return os_version.minorVersion >= 15;
}

static bool InstallSA(void)
{
    if (IsMacOSCatalinaOrNewer()) {
        fprintf(stderr, "The scripting-addition is not compatible with macOS Catalina and newer!\n");
        return false;
    }

    if (IsSAInstalled()) {
        if (!RemoveSA()) {
            return false;
        }
    }

    if (!CreateDirectoryStruct()) {
        goto cleanup;
    }

    if (!WriteTextFile(SASPlist, strlen(SASPlist), "/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Info.plist")) {
        goto cleanup;
    }

    if (!WriteTextFile(SASDef, strlen(SASDef), "/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/CHWMInjector.sdef")) {
        goto cleanup;
    }

    if (!WriteTextFile(SABPlist, strlen(SABPlist), "/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle/Contents/Info.plist")) {
        goto cleanup;
    }

    if (!WriteBinaryFile(bin_CHWMInjector_osax_Contents_MacOS_CHWMInjector,
                         bin_CHWMInjector_osax_Contents_MacOS_CHWMInjector_len,
                         "/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/MacOS/CHWMInjector")) {
        goto cleanup;
    }

    if (!WriteBinaryFile(bin_CHWMInjector_osax_Contents_Resources_chunkwm_sa_bundle_Contents_MacOS_chunkwm_sa,
                         bin_CHWMInjector_osax_Contents_Resources_chunkwm_sa_bundle_Contents_MacOS_chunkwm_sa_len,
                         "/System/Library/ScriptingAdditions/CHWMInjector.osax/Contents/Resources/chunkwm-sa.bundle/Contents/MacOS/chunkwm-sa")) {
        goto cleanup;
    }

    PrepBinaries();
    return true;

cleanup:
    RemoveSA();
    return false;
}

static int UninstallSA(void)
{
    if (!IsSAInstalled()) {
        return 0;
    }

    if (!RemoveSA()) {
        return -1;
    }

    return 1;
}

static int InjectSA(void)
{
    if (!IsSAInstalled()) {
        return 0;
    }

    SBApplication *SBApp = [[SBApplication applicationWithBundleIdentifier:@"com.apple.Dock"] retain];
    if (SBApp == nil) return -1;

    [SBApp setTimeout:10*60];
    [SBApp setSendMode:kAEWaitReply];
    [SBApp sendEvent:'ascr' id:'gdut' parameters:0];
    [SBApp setSendMode:kAENoReply];
    [SBApp sendEvent:'CHWM' id:'injc' parameters:0];
    [SBApp release];

    return 1;
}
