// Starts another program (WoWLauncher -> WoWEngine, WoWEngine -> WoWLauncher) and leaves it running.
//
// SDL_CreateProcess(args, false) hands the new process this one's standard input, output and error. WoWEngine
// and WoWLauncher are windowed programs (WIN32 subsystem), which have none, so on Windows SDL's DuplicateHandle()
// call fails with "The handle is invalid" and nothing starts. Here the new process gets no standard streams at
// all, and runs in the background so this one can close straight away.
#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace wow
{
    // Returns false with SDL_GetError() set if the program couldn't be started.
    inline bool LaunchDetached(const std::vector<std::string>& args)
    {
        std::vector<const char*> argv;
        for (const std::string& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);

        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props) return false;
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(argv.data()));
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
        SDL_Process* process = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        if (!process) return false;
        SDL_DestroyProcess(process); // the program keeps running; this only lets go of it
        return true;
    }
}
