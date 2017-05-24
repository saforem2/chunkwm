#include "plugin.h"

#include "../common/config/tokenize.h"
#include "../common/config/cvar.h"
#include "../common/misc/assert.h"
#include "../common/ipc/daemon.h"

#include "constants.h"

#include <stdio.h>
#include <string.h>

#define internal static

struct plugin_fs
{
    char *Absolutepath;
    char *Filename;
};

// NOTE(koekeishiya): Caller is responsible for freeing memory of returned pointer
internal char *
PluginFilenameFromAbsolutepath(char *Absolutepath)
{
    char *Filename = NULL;
    char *LastSlash = strrchr(Absolutepath, '/');

    if(LastSlash)
    {
        char *Extension = strrchr(Absolutepath, '.');
        if((Extension) && (strcmp(Extension, ".so") == 0))
        {
            Filename = strdup(LastSlash + 1);
        }
    }

    return Filename;
}

// NOTE(koekeishiya): Caller is responsible for freeing memory of returned pointer
internal char *
PluginAbsolutepathFromDirectory(char *Filename, char *Directory)
{
    char *Absolutepath = NULL;
    char *Extension = strrchr(Filename, '.');

    if((Extension) && (strcmp(Extension, ".so") == 0))
    {
        size_t DirectoryLength = strlen(Directory);
        size_t FilenameLength = strlen(Filename);
        size_t TotalLength = DirectoryLength + 1 + FilenameLength;

        Absolutepath = (char *) malloc(TotalLength + 1);
        Absolutepath[TotalLength] = '\0';

        memcpy(Absolutepath, Directory, DirectoryLength);
        memset(Absolutepath + DirectoryLength, '/', 1);
        memcpy(Absolutepath + DirectoryLength + 1, Filename, FilenameLength);
    }

    return Absolutepath;
}

internal bool
PopulatePluginPath(const char **Message, plugin_fs *PluginFs)
{
    char *Absolutepath, *Filename;
    token Token = GetToken(Message);
    char *Directory = CVarStringValue(CVAR_PLUGIN_DIR);

    if(Directory)
    {
        Filename = TokenToString(Token);
        Absolutepath = PluginAbsolutepathFromDirectory(Filename, Directory);
        if(!Absolutepath)
        {
            free(Filename);
            return false;
        }
    }
    else
    {
        Absolutepath = TokenToString(Token);
        Filename = PluginFilenameFromAbsolutepath(Absolutepath);
        if(!Filename)
        {
            free(Absolutepath);
            return false;
        }
    }

    PluginFs->Absolutepath = Absolutepath;
    PluginFs->Filename = Filename;
    return true;
}

internal void
DestroyPluginFS(plugin_fs *PluginFS)
{
    free(PluginFS->Absolutepath);
    free(PluginFS->Filename);
}

DAEMON_CALLBACK(DaemonCallback)
{
    token Type = GetToken(&Message);
    if(TokenEquals(Type, CVAR_PLUGIN_DIR))
    {
        token Token = GetToken(&Message);
        char *Directory = TokenToString(Token);
        UpdateCVar(CVAR_PLUGIN_DIR, Directory);
    }
    else if(TokenEquals(Type, CVAR_PLUGIN_HOTLOAD))
    {
        token Token = GetToken(&Message);
        int Status = TokenToInt(Token);
        UpdateCVar(CVAR_PLUGIN_HOTLOAD, Status);
    }
    else if(TokenEquals(Type, "load"))
    {
        plugin_fs PluginFS;
        if(PopulatePluginPath(&Message, &PluginFS))
        {
            struct stat Buffer;
            if(stat(PluginFS.Absolutepath, &Buffer) == 0)
            {
                LoadPlugin(PluginFS.Absolutepath, PluginFS.Filename);
            }
            else
            {
                fprintf(stderr, "chunkwm: plugin '%s' not found..\n", PluginFS.Absolutepath);
            }
            DestroyPluginFS(&PluginFS);
        }
    }
    else if(TokenEquals(Type, "unload"))
    {
        plugin_fs PluginFS;
        if(PopulatePluginPath(&Message, &PluginFS))
        {
            UnloadPlugin(PluginFS.Absolutepath, PluginFS.Filename);
            DestroyPluginFS(&PluginFS);
        }
    }
}
