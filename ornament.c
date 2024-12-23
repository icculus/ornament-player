#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdlib.h>  // for exit()

#include "theoraplay.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioStream *audio_stream = NULL;
static SDL_Texture *texture = NULL;
static THEORAPLAY_Decoder *decoder = NULL;
static const THEORAPLAY_VideoFrame *pending_frame = NULL;
static THEORAPLAY_Io theora_io;
static Uint64 baseticks = 0;
static bool audio_ready = false;
static Uint8 idle_intensity = 0;
static Uint8 idle_intensity_incr = 1;

static long theoraplayiobridge_read(THEORAPLAY_Io *io, void *buf, long buflen)
{
    SDL_IOStream *stream = (SDL_IOStream *) io->userdata;
    const size_t br = SDL_ReadIO(stream, buf, buflen);
    if ((br == 0) && (SDL_GetIOStatus(stream) != SDL_IO_STATUS_EOF)) {
        return -1;
    }
    return (long) br;
}

static long theoraplayiobridge_streamlen(THEORAPLAY_Io *io)
{
    return (long) SDL_GetIOSize((SDL_IOStream *) io->userdata);
}

static int theoraplayiobridge_seek(THEORAPLAY_Io *io, long absolute_offset)
{
    return SDL_SeekIO((SDL_IOStream *) io->userdata, absolute_offset, SDL_IO_SEEK_SET) != -1;
}

static void theoraplayiobridge_close(THEORAPLAY_Io *io)
{
    SDL_CloseIO((SDL_IOStream *) io->userdata);
}

static void *ornament_theoraplay_allocate(const THEORAPLAY_Allocator *allocator, unsigned int len)
{
    void *ptr = SDL_malloc((size_t) len);
    if (!ptr) {
        SDL_Quit();
        SDL_Log("OUT OF MEMORY!!!");
        exit(1);
    }
    return ptr;
}

static void ornament_theoraplay_deallocate(const THEORAPLAY_Allocator *allocator, void *ptr)
{
    SDL_free(ptr);
}

static bool setup_movie(const char *fname)
{
    char *fullpath = NULL;
    if (SDL_asprintf(&fullpath, "%s%s", SDL_GetBasePath(), fname) < 0) {
        return false;
    }

    SDL_IOStream *io = SDL_IOFromFile(fullpath, "rb");
    SDL_free(fullpath);

    THEORAPLAY_Allocator allocator;
    SDL_zero(allocator);
    allocator.allocate = ornament_theoraplay_allocate;
    allocator.deallocate = ornament_theoraplay_deallocate;
    allocator.userdata = NULL;

    SDL_zero(theora_io);
    theora_io.read = theoraplayiobridge_read;
    theora_io.streamlen = theoraplayiobridge_streamlen;
    theora_io.seek = theoraplayiobridge_seek;
    theora_io.close = theoraplayiobridge_close;
    theora_io.userdata = io;
    decoder = THEORAPLAY_startDecode(&theora_io, 30, THEORAPLAY_VIDFMT_IYUV, &allocator, 1);
    if (!decoder) {
        SDL_CloseIO(io);
        return SDL_SetError("Failed to start movie decoding!");
    }

    return true;
}


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Ryan's Ornament Player", "1.0", "org.icculus.ornament-player");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_HideCursor();

    //const SDL_WindowFlags flags = 0;
    const SDL_WindowFlags flags = SDL_WINDOW_FULLSCREEN;

    if (!SDL_CreateWindowAndRenderer("Ornament Player", 240, 240, flags, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    if (!setup_movie("ornament.ogv")) {
        SDL_Log("Failed to setup movie: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL, NULL, NULL);
    if (!audio_stream) {
        SDL_Log("Couldn't initialize audio: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    const THEORAPLAY_AudioPacket *audio = NULL;
    const Uint64 now = SDL_GetTicks();

    if (decoder) {
        if (!baseticks && THEORAPLAY_isInitialized(decoder)) {
            baseticks = now;
        }

        if (!THEORAPLAY_isDecoding(decoder)) {
            THEORAPLAY_seek(decoder, 0);  // start over.
            baseticks = now;
        }

        const Uint64 playback_now = now - baseticks;

        THEORAPLAY_pumpDecode(decoder, 5);

        if (pending_frame && (pending_frame->playms <= playback_now)) {
            if (texture) {
                SDL_UpdateTexture(texture, NULL, pending_frame->pixels, pending_frame->width);
            }
            THEORAPLAY_freeVideo(pending_frame);
            pending_frame = NULL;
        }

        if (!pending_frame) {
            const THEORAPLAY_VideoFrame *video;
            while ((video = THEORAPLAY_getVideo(decoder)) != NULL) {
                if (!texture) {
                    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, (int) video->width, (int) video->height);
                }
                if (video->playms >= playback_now) {
                    pending_frame = video;
                    break;
                }
                THEORAPLAY_freeVideo(video);  // we're behind, dump this frame and try to get another right now.
            }
        }

        const THEORAPLAY_AudioPacket *audio;
        while ((audio = THEORAPLAY_getAudio(decoder)) != NULL) {
            if (!audio_ready) {
                const SDL_AudioSpec spec = { SDL_AUDIO_F32, audio->channels, audio->freq };
                SDL_SetAudioStreamFormat(audio_stream, &spec, NULL);
                SDL_ResumeAudioStreamDevice(audio_stream);
                audio_ready = true;
            }
            SDL_PutAudioStreamData(audio_stream, audio->samples, (int) (audio->frames * audio->channels * sizeof (float)));
            THEORAPLAY_freeAudio(audio);  // dump this, the game is going to seek at startup anyhow.
        }
    }

    if (!texture) {   // still waiting on initial load.
        idle_intensity += idle_intensity_incr;
        if ( ((idle_intensity_incr > 0) && (idle_intensity == 255)) || ((idle_intensity_incr < 0) && (idle_intensity == 0)) ) {
            idle_intensity_incr = -idle_intensity_incr;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, idle_intensity, 255);
        SDL_RenderClear(renderer);
    } else {
        SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 1.0f);
        SDL_RenderClear(renderer);
        if (texture) {
            SDL_RenderTexture(renderer, texture, NULL, NULL);
        }
    }

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (pending_frame) {
        THEORAPLAY_freeVideo(pending_frame);
    }

    if (decoder) {
        THEORAPLAY_stopDecode(decoder);
    }

    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

// end of ornament.c ...

