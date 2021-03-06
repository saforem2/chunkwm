#include "plugin.h"

#include "../common/config/tokenize.h"
#include "../common/config/cvar.h"
#include "../common/misc/assert.h"
#include "../common/ipc/daemon.h"

#include "constants.h"

#include <stdio.h>
#include <string.h>

DAEMON_CALLBACK(DaemonCallback)
{
    token Type = GetToken(&Message);
    if(TokenEquals(Type, CVAR_PLUGIN_HOTLOAD))
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
            char *Extension = strrchr(Filename, '.');

            if((Extension) && (strcmp(Extension, ".so") == 0))
            {
                size_t DirectoryLength = strlen(Directory);
                size_t FilenameLength = strlen(Filename);
                size_t TotalLength = DirectoryLength + 1 + FilenameLength;

                char Absolutepath[TotalLength + 1];
                Absolutepath[TotalLength] = '\0';

                memcpy(Absolutepath, Directory, DirectoryLength);
                memset(Absolutepath + DirectoryLength, '/', 1);
                memcpy(Absolutepath + DirectoryLength + 1, Filename, FilenameLength);

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

            free(Filename);
        }
        else
        {
            char *Absolutepath = TokenToString(Token);
            char *LastSlash = strrchr(Absolutepath, '/');

            if(LastSlash)
            {
                char *Extension = strrchr(Absolutepath, '.');
                if((Extension) && (strcmp(Extension, ".so") == 0))
                {
                    char *Filename = LastSlash + 1;

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
            char *Extension = strrchr(Filename, '.');

            if((Extension) && (strcmp(Extension, ".so") == 0))
            {
                size_t DirectoryLength = strlen(Directory);
                size_t FilenameLength = strlen(Filename);
                size_t TotalLength = DirectoryLength + 1 + FilenameLength;

                char Absolutepath[TotalLength + 1];
                memcpy(Absolutepath, Directory, DirectoryLength);
                memset(Absolutepath + DirectoryLength, '/', 1);
                memcpy(Absolutepath + DirectoryLength + 1, Filename, FilenameLength);

                UnloadPlugin(Absolutepath, Filename);
            }
        }
        else
        {
            char *Absolutepath = TokenToString(Token);
            char *LastSlash = strrchr(Absolutepath, '/');

            if(LastSlash)
            {
                char *Extension = strrchr(Absolutepath, '.');
                if((Extension) && (strcmp(Extension, ".so") == 0))
                {
                    char *Filename = LastSlash + 1;
                    UnloadPlugin(Absolutepath, Filename);
                }
            }
        }
    }
    else if(TokenEquals(Type, CVAR_PLUGIN_DIR))
    {
        token Token = GetToken(&Message);
        char *Directory = TokenToString(Token);
        UpdateCVar(CVAR_PLUGIN_DIR, Directory);
    }
}
