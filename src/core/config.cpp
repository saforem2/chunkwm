#include "config.h"
#include "plugin.h"
#include "clog.h"

#include "../common/config/tokenize.h"
#include "../common/misc/assert.h"
#include "../common/ipc/daemon.h"

#include "dispatch/event.h"

#include "constants.h"
#include "cvar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define internal static

internal inline bool
StringEquals(const char *A, const char *B)
{
    return (strcmp(A, B) == 0);
}

// NOTE(koekeishiya): Caller is responsible for freeing memory of returned pointer
internal char *
PluginFilenameFromAbsolutepath(char *Absolutepath)
{
    char *Filename = NULL;
    char *LastSlash = strrchr(Absolutepath, '/');

    if (LastSlash) {
        char *Extension = strrchr(Absolutepath, '.');
        if ((Extension) && (strcmp(Extension, ".so") == 0)) {
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

    if ((Extension) && (strcmp(Extension, ".so") == 0)) {
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

internal char *
SanitizePluginFilename(char *Filename)
{
    char *Result = Filename;
    char *LastSlash = strrchr(Filename, '/');
    if (LastSlash) {
        size_t FilenameLength = strlen(LastSlash + 1);
        Result = (char *) malloc(FilenameLength + 1);
        Result[FilenameLength] = '\0';
        memcpy(Result, LastSlash + 1, FilenameLength);
        free(Filename);
    }
    return Result;
}

internal bool
PopulatePluginPath(const char **Message, plugin_fs *PluginFs)
{
    char *Absolutepath, *Filename;
    token Token = GetToken(Message);
    char *Directory = CVarStringValue(CVAR_PLUGIN_DIR);

    if (Directory) {
        Filename = SanitizePluginFilename(TokenToString(Token));
        Absolutepath = PluginAbsolutepathFromDirectory(Filename, Directory);
        if (!Absolutepath) {
            free(Filename);
            return false;
        }
    } else {
        Absolutepath = TokenToString(Token);
        Filename = PluginFilenameFromAbsolutepath(Absolutepath);
        if (!Filename) {
            free(Absolutepath);
            return false;
        }
    }

    PluginFs->Absolutepath = Absolutepath;
    PluginFs->Filename = Filename;
    return true;
}

internal bool
ChunkwmDaemonDelegate(const char *Message, chunkwm_delegate *Delegate)
{
    bool Success = false;
    token IdentifierToken = GetToken(&Message);

    if (IdentifierToken.Length > 0) {
        char *Identifier = TokenToString(IdentifierToken);
        char Target[64];
        char Command[64];

        Success = (sscanf(Identifier, "%[^:]%*[:]%s", Target, Command) == 2);
        if (Success) {
            Delegate->Target = strdup(Target);
            Delegate->Command = strdup(Command);
            Delegate->Message = strdup(Message);
        }

        free(Identifier);
    }

    return Success;
}

internal void
HandleCore(chunkwm_delegate *Delegate)
{
    if (StringEquals(Delegate->Command, CVAR_PLUGIN_DIR)) {
        token Token = GetToken(&Delegate->Message);
        char *Directory = TokenToString(Token);
        CreateCVar(CVAR_PLUGIN_DIR, Directory);
        free(Directory);
    } else if (StringEquals(Delegate->Command, CVAR_PLUGIN_HOTLOAD)) {
        token Token = GetToken(&Delegate->Message);
        int Status = TokenToInt(Token);
        UpdateCVar(CVAR_PLUGIN_HOTLOAD, Status);
    } else if (StringEquals(Delegate->Command, CVAR_LOG_FILE)) {
        if (c_log_output_file == stdout) {
            token Token = GetToken(&Delegate->Message);
            if (TokenEquals(Token, "stdout")) {
                c_log_output_file = stdout;
            } else if (TokenEquals(Token, "stderr")) {
                c_log_output_file = stderr;
            } else {
                char *OutputFile = TokenToString(Token);
                FILE *Handle = fopen(OutputFile, "w");
                if (Handle) {
                    c_log_output_file = Handle;
                }
                free(OutputFile);
            }
        }
    } else if (StringEquals(Delegate->Command, CVAR_LOG_LEVEL)) {
        token Token = GetToken(&Delegate->Message);
        if (TokenEquals(Token, "none")) {
            c_log_active_level = C_LOG_LEVEL_NONE;
        } else if (TokenEquals(Token, "debug")) {
            c_log_active_level = C_LOG_LEVEL_DEBUG;
        } else if (TokenEquals(Token, "warn")) {
            c_log_active_level = C_LOG_LEVEL_WARN;
        } else if (TokenEquals(Token, "error")) {
            c_log_active_level = C_LOG_LEVEL_ERROR;
        } else if (TokenEquals(Token, "profile")) {
            c_log_active_level = C_LOG_LEVEL_PROFILE;
        }
    } else if (StringEquals(Delegate->Command, "load")) {
        plugin_fs *PluginFS = (plugin_fs *) malloc(sizeof(plugin_fs));
        if (PopulatePluginPath(&Delegate->Message, PluginFS)) {
            struct stat Buffer;
            if (lstat(PluginFS->Absolutepath, &Buffer) == 0) {
                if (S_ISLNK(Buffer.st_mode)) {
                    char *ResolvedPath = (char *) malloc(PATH_MAX);
                    realpath(PluginFS->Absolutepath, ResolvedPath);
                    free(PluginFS->Absolutepath);
                    PluginFS->Absolutepath = ResolvedPath;
                }
                ConstructEvent(ChunkWM_PluginLoad, PluginFS);
            } else {
                c_log(C_LOG_LEVEL_WARN, "chunkwm: plugin '%s' not found..\n", PluginFS->Absolutepath);
                DestroyPluginFS(PluginFS);
                free(PluginFS);
            }
        } else {
            free(PluginFS);
        }
    } else if (StringEquals(Delegate->Command, "unload")) {
        plugin_fs *PluginFS = (plugin_fs *) malloc(sizeof(plugin_fs));
        if (PopulatePluginPath(&Delegate->Message, PluginFS)) {
            ConstructEvent(ChunkWM_PluginUnload, PluginFS);
        } else {
            free(PluginFS);
        }
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm: invalid command '%s::%s'\n", Delegate->Target, Delegate->Command);
    }

    CloseSocket(Delegate->SockFD);
    free(Delegate->Target);
    free(Delegate->Command);
    free(Delegate);
}

internal inline bool
ValidToken(token *Token)
{
    bool Result = Token->Length > 0;
    return Result;
}

internal void
SetCVar(const char **Message)
{
    token NameToken = GetToken(Message);
    if (ValidToken(&NameToken)) {
        token ValueToken = GetToken(Message);
        if (ValidToken(&ValueToken)) {
            char *Name = TokenToString(NameToken);
            char *Value = TokenToString(ValueToken);
            UpdateCVar(Name, Value);
            free(Name);
            free(Value);
        } else {
            c_log(C_LOG_LEVEL_WARN, "chunkwm: missing value for cvar '%.*s'.\n", NameToken.Length, NameToken.Length);
        }
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm: missing cvar name.\n");
    }
}

internal void
GetCVar(const char **Message, int SockFD)
{
    token NameToken = GetToken(Message);
    if (ValidToken(&NameToken)) {
        char *Name = TokenToString(NameToken);
        char *Value = CVarStringValue(Name);
        if (Value) {
            WriteToSocket(Value, SockFD);
        }
        free(Name);
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm: missing cvar name.\n");
    }
}

internal void
HandleCVar(chunkwm_delegate *Delegate, const char **Message)
{
    token Type = GetToken(Message);
    if (TokenEquals(Type, "set")) {
        SetCVar(Message);
    } else if (TokenEquals(Type, "get")) {
        GetCVar(Message, Delegate->SockFD);
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm: invalid command '%.*s %s'\n", Type.Length, Type.Text, *Message);
    }
    CloseSocket(Delegate->SockFD);
    free(Delegate);
}

DAEMON_CALLBACK(DaemonCallback)
{
    chunkwm_delegate *Delegate = (chunkwm_delegate *) malloc(sizeof(chunkwm_delegate));
    memset(Delegate, 0, sizeof(chunkwm_delegate));
    Delegate->SockFD = SockFD;

    if (ChunkwmDaemonDelegate(Message, Delegate)) {
        if (StringEquals(Delegate->Target, "core")) {
            HandleCore(Delegate);
        } else {
            ConstructEvent(ChunkWM_PluginCommand, Delegate);
        }
    } else {
        HandleCVar(Delegate, &Message);
    }
}
