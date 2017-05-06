#include "plugin.h"

#include "../common/config/tokenize.h"
#include "../common/config/cvar.h"
#include "../common/misc/assert.h"
#include "../common/ipc/daemon.h"

#include "constants.h"

#include <stdio.h>
#include <string.h>

#define internal static

// NOTE(koekeishiya): The char * we return points to a location
// in the provided absolutepath string, thus, it should NOT be freed.
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
            Filename = LastSlash + 1;
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
        token Token = GetToken(&Message);
        char *Directory = CVarStringValue(CVAR_PLUGIN_DIR);
        if(Directory)
        {
            char *Filename = TokenToString(Token);
            char *Absolutepath = PluginAbsolutepathFromDirectory(Filename, Directory);
            if(Absolutepath)
            {
                struct stat Buffer;
                if(stat(Absolutepath, &Buffer) == 0)
                {
                    LoadPlugin(Absolutepath, Filename);
                }
                else
                {
                    fprintf(stderr, "chunkwm: plugin '%s' not found..\n", Absolutepath);
                }
                free(Absolutepath);
            }
            free(Filename);
        }
        else
        {
            char *Absolutepath = TokenToString(Token);
            char *Filename = PluginFilenameFromAbsolutepath(Absolutepath);
            if(Filename)
            {
                struct stat Buffer;
                if(stat(Absolutepath, &Buffer) == 0)
                {
                    LoadPlugin(Absolutepath, Filename);
                }
                else
                {
                    fprintf(stderr, "chunkwm: plugin '%s' not found..\n", Absolutepath);
                }
            }
            free(Absolutepath);
        }
    }
    else if(TokenEquals(Type, "unload"))
    {
        token Token = GetToken(&Message);
        char *Directory = CVarStringValue(CVAR_PLUGIN_DIR);
        if(Directory)
        {
            char *Filename = TokenToString(Token);
            char *Absolutepath = PluginAbsolutepathFromDirectory(Filename, Directory);
            if(Absolutepath)
            {
                UnloadPlugin(Absolutepath, Filename);
                free(Absolutepath);
            }
            free(Filename);
        }
        else
        {
            char *Absolutepath = TokenToString(Token);
            char *Filename = PluginFilenameFromAbsolutepath(Absolutepath);
            if(Filename)
            {
                UnloadPlugin(Absolutepath, Filename);
            }
            free(Absolutepath);
        }
    }
}
