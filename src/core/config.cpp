#include "plugin.h"

#include "../common/config/tokenize.h"
#include "../common/misc/assert.h"
#include "../common/ipc/daemon.h"

#include "constants.h"
#include "cvar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal static

struct chunkwm_delegate
{
    char *Target;
    char *Command;
    const char *Message;
};

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

internal bool
ChunkwmDaemonDelegate(const char *Message, chunkwm_delegate *Delegate)
{
    bool Success = false;
    token IdentifierToken = GetToken(&Message);

    if(IdentifierToken.Length > 0)
    {
        char *Identifier = TokenToString(IdentifierToken);
        char Target[64];
        char Command[64];

        Success = (sscanf(Identifier, "%[^:]%*[:]%s", Target, Command) == 2);
        if(Success)
        {
            Delegate->Target = strdup(Target);
            Delegate->Command = strdup(Command);
            Delegate->Message = Message;
        }

        free(Identifier);
    }

    return Success;
}

internal void
DestroyChunkwmDaemonDelegate(chunkwm_delegate *Delegate)
{
    free(Delegate->Target);
    free(Delegate->Command);
}

internal inline bool
StringEquals(const char *A, const char *B)
{
    return (strcmp(A, B) == 0);
}

internal void
HandleCore(chunkwm_delegate *Delegate)
{
    if(StringEquals(Delegate->Command, CVAR_PLUGIN_DIR))
    {
        token Token = GetToken(&Delegate->Message);
        char *Directory = TokenToString(Token);
        UpdateCVar(CVAR_PLUGIN_DIR, Directory);
        free(Directory);
    }
    else if(StringEquals(Delegate->Command, CVAR_PLUGIN_HOTLOAD))
    {
        token Token = GetToken(&Delegate->Message);
        int Status = TokenToInt(Token);
        UpdateCVar(CVAR_PLUGIN_HOTLOAD, Status);
    }
    else if(StringEquals(Delegate->Command, "load"))
    {
        plugin_fs PluginFS;
        if(PopulatePluginPath(&Delegate->Message, &PluginFS))
        {
            struct stat Buffer;
            if(lstat(PluginFS.Absolutepath, &Buffer) == 0)
            {
                if(S_ISLNK(Buffer.st_mode))
                {
                    char *ResolvedPath = (char *) malloc(PATH_MAX);
                    realpath(PluginFS.Absolutepath, ResolvedPath);
                    LoadPlugin(ResolvedPath, PluginFS.Filename);
                    free(ResolvedPath);
                }
                else
                {
                    LoadPlugin(PluginFS.Absolutepath, PluginFS.Filename);
                }
            }
            else
            {
                fprintf(stderr, "chunkwm: plugin '%s' not found..\n", PluginFS.Absolutepath);
            }
            DestroyPluginFS(&PluginFS);
        }
    }
    else if(StringEquals(Delegate->Command, "unload"))
    {
        plugin_fs PluginFS;
        if(PopulatePluginPath(&Delegate->Message, &PluginFS))
        {
            UnloadPlugin(PluginFS.Absolutepath, PluginFS.Filename);
            DestroyPluginFS(&PluginFS);
        }
    }
    else
    {
        fprintf(stderr, "chunkwm: invalid command '%s::%s'\n", Delegate->Target, Delegate->Command);
    }
}

internal void
HandlePlugin(int SockFD, chunkwm_delegate *Delegate)
{
    plugin *Plugin = GetPluginFromFilename(Delegate->Target);
    if(Plugin)
    {
        chunkwm_payload Payload = { SockFD, Delegate->Command, Delegate->Message };
        Plugin->Run("chunkwm_daemon_command", (void *) &Payload);
    }
    else
    {
        fprintf(stderr, "chunkwm: plugin '%s' is not loaded\n", Delegate->Target);
    }
}

internal inline bool
ValidToken(token *Token, const char *Format, ...)
{
    bool Result = Token->Length > 0;
    if(!Result)
    {
        va_list Args;
        va_start(Args, Format);
        vfprintf(stderr, Format, Args);
        va_end(Args);
    }
    return Result;
}

internal void
SetCVar(const char **Message)
{
    token NameToken = GetToken(Message);
    if(ValidToken(&NameToken, "chunkwm: missing cvar name !!!\n"))
    {
        token ValueToken = GetToken(Message);
        if(ValidToken(&ValueToken, "chunkwm: missing value for cvar '%.*s'\n", NameToken.Length, NameToken.Text))
        {
            char *Name = TokenToString(NameToken);
            char *Value = TokenToString(ValueToken);
            UpdateCVar(Name, Value);
            free(Name);
            free(Value);
        }
    }
}

internal void
GetCVar(const char **Message, int SockFD)
{
    token NameToken = GetToken(Message);
    if(ValidToken(&NameToken, "chunkwm: missing cvar name !!!\n"))
    {
        char *Name = TokenToString(NameToken);
        char *Value = CVarStringValue(Name);
        WriteToSocket(Value, SockFD);
        free(Name);
    }
}

DAEMON_CALLBACK(DaemonCallback)
{
    chunkwm_delegate Delegate;
    if(ChunkwmDaemonDelegate(Message, &Delegate))
    {
        if(StringEquals(Delegate.Target, "core"))
        {
            HandleCore(&Delegate);
        }
        else
        {
            HandlePlugin(SockFD, &Delegate);
        }
        DestroyChunkwmDaemonDelegate(&Delegate);
    }
    else
    {
        token Type = GetToken(&Message);
        if(TokenEquals(Type, "set"))
        {
            SetCVar(&Message);
        }
        else if(TokenEquals(Type, "get"))
        {
            GetCVar(&Message, SockFD);
        }
        else
        {
            fprintf(stderr, "chunkwm: invalid command '%.*s %s'\n", Type.Length, Type.Text, Message);
        }
    }
}
