#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// AudioManager.hpp  —  Spatial sound, music streaming, and volume groups
//
// SFML AUDIO CONCEPTS:
//   sf::SoundBuffer — PCM data loaded into RAM (short sound effects).
//   sf::Sound       — Plays a SoundBuffer; multiple Sounds can share one buffer.
//   sf::Music       — Streams from disk (large files like background music).
//
// THIS MANAGER ADDS:
//   • Voice pool: a fixed set of sf::Sound instances. When you play a sound,
//     the manager finds a free (stopped) voice and uses it. This avoids
//     creating/destroying sf::Sound objects constantly.
//   • Volume groups (buses): "sfx", "music", "ui" each have a master volume.
//     Changing "sfx" volume scales all sound effects at once.
//   • Spatial audio: play a sound at a world position; it fades with distance.
//   • Music crossfade: smoothly fade between two music tracks.
//
// WHY A VOICE POOL?
//   sf::Sound is not cheap to construct. Keeping a fixed pool of, say, 32
//   sounds means we never allocate at runtime. If all voices are busy, we
//   either steal the oldest or silently drop the new sound.
// ─────────────────────────────────────────────────────────────────────────────

#include "AssetManager.hpp"
#include <SFML/Audio.hpp>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <cmath>

namespace engine {

// Number of simultaneously playing sound effects.
inline constexpr std::size_t VOICE_POOL_SIZE = 32;

// Volume group names (buses).
enum class AudioBus { Master, SFX, Music, UI };

class AudioManager {
public:
    explicit AudioManager(AssetManager& assets)
        : m_assets(assets)
    {
        // Pre-create all voice slots. They start in Stopped state.
        // std::array default-constructs all sf::Sound objects.
    }

    // ── Volume control ────────────────────────────────────────────────────────

    // Volume in [0, 100].
    void setVolume(AudioBus bus, float volume) {
        m_busVolumes[static_cast<int>(bus)] = std::clamp(volume, 0.f, 100.f);
    }

    float getVolume(AudioBus bus) const {
        return m_busVolumes[static_cast<int>(bus)];
    }

    // ── Sound effects ─────────────────────────────────────────────────────────

    // Play a loaded sound effect (from AssetManager) on the SFX bus.
    // Returns the voice index (for pitch/volume tweaks), or -1 if pool full.
    int playSound(const std::string& id,
                  float pitchVariance = 0.f, // ± variance for non-repetitive audio
                  AudioBus bus = AudioBus::SFX)
    {
        sf::SoundBuffer* buf = nullptr;
        try { buf = &m_assets.getSound(id); }
        catch (...) { return -1; }

        int idx = findFreeVoice();
        if (idx < 0) idx = stealOldestVoice(); // Steal if pool full.
        if (idx < 0) return -1;

        m_voices[idx].setBuffer(*buf);
        m_voices[idx].setVolume(effectiveVolume(bus));
        m_voiceBus[idx] = bus;
        m_voiceStartTime[idx] = m_clock.getElapsedTime().asSeconds();

        if (pitchVariance > 0.f) {
            // Small random pitch shift prevents repetitive sounds feeling robotic.
            float pitch = 1.f + (static_cast<float>(std::rand()) / RAND_MAX * 2.f - 1.f)
                              * pitchVariance;
            m_voices[idx].setPitch(pitch);
        } else {
            m_voices[idx].setPitch(1.f);
        }

        m_voices[idx].play();
        return idx;
    }

    // Spatial sound: attenuates based on distance from a listener position.
    // listenerPos is typically the camera/player world position.
    int playSoundAt(const std::string& id,
                    sf::Vector2f soundPos,
                    sf::Vector2f listenerPos,
                    float maxDistance = 600.f,
                    float pitchVariance = 0.f)
    {
        float dist = std::sqrt(
            (soundPos.x - listenerPos.x) * (soundPos.x - listenerPos.x) +
            (soundPos.y - listenerPos.y) * (soundPos.y - listenerPos.y)
        );
        if (dist >= maxDistance) return -1; // Too far — don't bother.

        // Linear falloff: volume 100% at dist=0, 0% at dist=maxDistance.
        float spatialVolume = 1.f - (dist / maxDistance);
        int idx = playSound(id, pitchVariance);
        if (idx >= 0) {
            float current = m_voices[idx].getVolume();
            m_voices[idx].setVolume(current * spatialVolume);
        }
        return idx;
    }

    // ── Music streaming ───────────────────────────────────────────────────────

    // Play background music (streamed from disk).
    // crossfadeDuration > 0 fades out the previous track while fading in the new one.
    void playMusic(const std::string& filepath,
                   bool  loop           = true,
                   float crossfadeSec   = 1.5f)
    {
        if (m_currentMusic && m_currentMusic->getStatus() == sf::Music::Playing) {
            // Start a crossfade: hand the current track to the outgoing slot.
            m_outgoingMusic = std::move(m_currentMusic);
            m_crossfadeTimer    = 0.f;
            m_crossfadeDuration = crossfadeSec;
        }

        // sf::Music must be opened, not loaded entirely into RAM.
        // unique_ptr: AudioManager owns this music instance.
        m_currentMusic = std::make_unique<sf::Music>();
        if (!m_currentMusic->openFromFile(filepath)) {
            m_currentMusic.reset();
            return;
        }
        m_currentMusic->setLoop(loop);
        m_currentMusic->setVolume(crossfadeSec > 0.f ? 0.f : effectiveVolume(AudioBus::Music));
        m_currentMusic->play();
    }

    void stopMusic(float fadeDuration = 1.f) {
        if (!m_currentMusic) return;
        m_outgoingMusic     = std::move(m_currentMusic);
        m_crossfadeTimer    = 0.f;
        m_crossfadeDuration = fadeDuration;
        // m_currentMusic is now null, so no fade-in.
    }

    void pauseMusic() { if (m_currentMusic) m_currentMusic->pause(); }
    void resumeMusic() { if (m_currentMusic) m_currentMusic->play(); }

    // ── Update (call once per frame) ──────────────────────────────────────────
    // Handles crossfade, bus volume changes applied to live voices, etc.
    void update(float dt) {
        // Crossfade update.
        if (m_crossfadeDuration > 0.f) {
            m_crossfadeTimer += dt;
            float t = std::min(m_crossfadeTimer / m_crossfadeDuration, 1.f);

            if (m_outgoingMusic) {
                m_outgoingMusic->setVolume(effectiveVolume(AudioBus::Music) * (1.f - t));
                if (t >= 1.f) {
                    m_outgoingMusic->stop();
                    m_outgoingMusic.reset();
                    m_crossfadeDuration = 0.f;
                }
            }
            if (m_currentMusic) {
                m_currentMusic->setVolume(effectiveVolume(AudioBus::Music) * t);
            }
        } else if (m_currentMusic) {
            m_currentMusic->setVolume(effectiveVolume(AudioBus::Music));
        }
    }

    // Stop all voices immediately.
    void stopAll() {
        for (auto& v : m_voices) v.stop();
        if (m_currentMusic)  m_currentMusic->stop();
        if (m_outgoingMusic) m_outgoingMusic->stop();
    }

private:
    float effectiveVolume(AudioBus bus) const {
        float master = m_busVolumes[static_cast<int>(AudioBus::Master)];
        float local  = m_busVolumes[static_cast<int>(bus)];
        return master * local / 100.f; // Both in [0,100], combined → [0,100].
    }

    int findFreeVoice() const {
        for (int i = 0; i < static_cast<int>(VOICE_POOL_SIZE); ++i) {
            if (m_voices[i].getStatus() == sf::Sound::Stopped) return i;
        }
        return -1;
    }

    // Steal the voice that has been playing longest (least likely to be heard).
    int stealOldestVoice() const {
        int oldest = 0;
        float oldestTime = m_voiceStartTime[0];
        for (int i = 1; i < static_cast<int>(VOICE_POOL_SIZE); ++i) {
            if (m_voiceStartTime[i] < oldestTime) {
                oldestTime = m_voiceStartTime[i];
                oldest = i;
            }
        }
        return oldest;
    }

    AssetManager& m_assets;

    // Voice pool: fixed array, zero runtime allocation.
    std::array<sf::Sound, VOICE_POOL_SIZE>   m_voices;
    std::array<AudioBus,  VOICE_POOL_SIZE>   m_voiceBus        {};
    std::array<float,     VOICE_POOL_SIZE>   m_voiceStartTime  {};

    // Bus volumes: indexed by AudioBus enum cast to int.
    std::array<float, 4> m_busVolumes { 100.f, 100.f, 100.f, 100.f };

    // Music — unique_ptr because sf::Music is non-copyable and large.
    std::unique_ptr<sf::Music> m_currentMusic;
    std::unique_ptr<sf::Music> m_outgoingMusic;
    float m_crossfadeTimer    { 0.f };
    float m_crossfadeDuration { 0.f };

    sf::Clock m_clock; // Used to track voice start times for stealing.
};

} // namespace engine
