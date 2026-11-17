#pragma once
// Background music: one looping track per part of the story, with a soft cross-fade
// when the track changes (miniaudio, third_party/miniaudio). Licences and authors of
// the tracks: assets/music/SOURCE.md.
#include <memory>

enum class Track {
    None,
    Intro,       // the title screen (optional: assets/music/intro.ogg, silent if missing)
    HallQuiet,   // the throne hall before the key: low wind and water drops
    HallKey,     // the throne hall after the key is taken: "Throne Room"
    Garden,      // the inner garden: "Cathedral in the forest"
    Count
};

class Music {
public:
    Music();
    ~Music();
    // Opens the sound device and the files. Returns false if there is no sound at all;
    // the game still runs, only silently. A missing file only silences its own track.
    bool init();
    // Fade the current track out and `track` in (looping). Asking for the track that
    // already plays does nothing, so this can be called every frame.
    void play(Track track);
    void toggleMute();          // M
    bool muted() const { return isMuted; }
    void destroy();

private:
    struct Impl;                // miniaudio's types stay inside music.cpp
    std::unique_ptr<Impl> impl;
    Track current = Track::None;
    bool isMuted = false;
};
