// Background music with miniaudio (public domain / MIT-0, one header).
// stb_vorbis decodes the .ogg files; miniaudio decodes .mp3 itself.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include "music.h"
#include <filesystem>
#include <iostream>

namespace {

// One file and one loudness per track. The tracks were mixed at different levels,
// so each gets its own volume (1 = as recorded).
struct TrackFile {
    const char* path;
    float volume;
};
const TrackFile trackFiles[int(Track::Count)] = {
    {nullptr, 0.0f},                                           // None
    {"assets/music/intro.ogg", 0.7f},                          // Intro (optional)
    {"assets/music/hall_dungeon_ambience.ogg", 0.9f},          // HallQuiet
    {"assets/music/hall_throne_room.mp3", 0.45f},              // HallKey
    {"assets/music/garden_cathedral_in_the_forest.ogg", 0.7f}, // Garden
};
constexpr ma_uint64 fadeMs = 1500;   // length of the cross-fade

} // namespace

struct Music::Impl {
    ma_engine engine{};
    ma_sound sounds[int(Track::Count)]{};
    bool loaded[int(Track::Count)] = {};
};

Music::Music() = default;
Music::~Music() { destroy(); }

bool Music::init() {
    impl = std::make_unique<Impl>();
    if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS) {
        std::cerr << "No sound device: the game runs without music.\n";
        impl.reset();
        return false;
    }
    for (int i = 1; i < int(Track::Count); ++i) {
        TrackFile file = trackFiles[i];
        // The title screen's track may be .ogg or .mp3.
        if (Track(i) == Track::Intro && !std::filesystem::exists(file.path)) file.path = "assets/music/intro.mp3";
        if (!std::filesystem::exists(file.path)) {
            if (Track(i) != Track::Intro) std::cerr << "Music file not found: " << file.path << '\n';
            continue;
        }
        // STREAM: decode a little at a time while playing, instead of the whole file at once.
        if (ma_sound_init_from_file(&impl->engine, file.path, MA_SOUND_FLAG_STREAM, nullptr, nullptr,
                                    &impl->sounds[i]) != MA_SUCCESS) {
            std::cerr << "Could not open music: " << file.path << '\n';
            continue;
        }
        ma_sound_set_looping(&impl->sounds[i], MA_TRUE);
        ma_sound_set_volume(&impl->sounds[i], file.volume);
        impl->loaded[i] = true;
    }
    return true;
}

void Music::play(Track track) {
    if (!impl || track == current) return;
    // The old track fades out and stops; the new one starts from the beginning and fades in.
    if (current != Track::None && impl->loaded[int(current)])
        ma_sound_stop_with_fade_in_milliseconds(&impl->sounds[int(current)], fadeMs);
    current = track;
    if (track != Track::None && impl->loaded[int(track)]) {
        ma_sound* sound = &impl->sounds[int(track)];
        ma_sound_seek_to_pcm_frame(sound, 0);
        ma_sound_set_fade_in_milliseconds(sound, 0.0f, 1.0f, fadeMs);
        ma_sound_start(sound);
    }
}

void Music::toggleMute() {
    if (!impl) return;
    isMuted = !isMuted;
    ma_engine_set_volume(&impl->engine, isMuted ? 0.0f : 1.0f);   // the master volume of all tracks
}

void Music::destroy() {
    if (!impl) return;
    for (int i = 1; i < int(Track::Count); ++i)
        if (impl->loaded[i]) ma_sound_uninit(&impl->sounds[i]);
    ma_engine_uninit(&impl->engine);
    impl.reset();
}
