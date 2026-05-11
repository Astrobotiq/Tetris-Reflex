#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// AudioManager.hpp  —  SFML 3 uyumlu ses yöneticisi
//
// SFML 3 değişiklikleri:
//   • sf::Sound default constructor kaldırıldı → voice pool std::list kullanır
//   • sf::Music::setLoop() kaldırıldı → constructor'da dosya alınır
//   • sf::Music constructor'ı dosya yolunu alır, openFromFile yok
// ─────────────────────────────────────────────────────────────────────────────

#include <SFML/Audio.hpp>
#include <algorithm>
#include <list>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <cmath>
#include <cstdlib>

namespace engine {
    enum class AudioBus { Master, SFX, Music, UI };

    class AudioManager {
    public:
        AudioManager() = default;

        // ── Ses yükleme ───────────────────────────────────────────────────────────
        bool loadSound(const std::string &id, const std::string &path) {
            sf::SoundBuffer buf;
            if (!buf.loadFromFile(path)) {
                std::cerr << "AudioManager: ses yuklenemedi -> " << path << '\n';
                return false;
            }
            m_buffers.emplace(id, std::move(buf));
            return true;
        }

        // ── Volüm kontrolü ────────────────────────────────────────────────────────
        void setVolume(AudioBus bus, float volume) {
            m_busVolumes[static_cast<int>(bus)] = std::clamp(volume, 0.f, 100.f);
        }

        float getVolume(AudioBus bus) const {
            return m_busVolumes[static_cast<int>(bus)];
        }

        // ── Ses efekti çal ────────────────────────────────────────────────────────
        // pitchVariance > 0: küçük rastgele pitch kayması (tekrarlı sesleri önler)
        void playSound(const std::string &id,
                       float pitchVariance = 0.f,
                       AudioBus bus = AudioBus::SFX) {
            auto it = m_buffers.find(id);
            if (it == m_buffers.end()) return;

            // Bitmiş sesleri temizle
            m_voices.remove_if([](const sf::Sound &s) {
                return s.getStatus() == sf::Sound::Status::Stopped;
            });

            // Max 32 eş zamanlı ses
            if (m_voices.size() >= 32) {
                m_voices.pop_front(); // En eskiyi sil
            }

            // SFML 3: sf::Sound buffer ile oluşturulmalı
            m_voices.emplace_back(it->second);
            sf::Sound &voice = m_voices.back();
            voice.setVolume(effectiveVolume(bus));

            if (pitchVariance > 0.f) {
                float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
                float pitch = 1.f + (r * 2.f - 1.f) * pitchVariance;
                voice.setPitch(pitch);
            }

            voice.play();
        }

        // ── Müzik streaming ───────────────────────────────────────────────────────
        // SFML 3: sf::Music constructor dosya yolunu alır, setLoop() yok
        void playMusic(const std::string &filepath,
                       bool loop = true,
                       float crossfadeSec = 1.5f) {
            if (m_currentMusic &&
                m_currentMusic->getStatus() == sf::Music::Status::Playing) {
                m_outgoingMusic = std::move(m_currentMusic);
                m_crossfadeTimer = 0.f;
                m_crossfadeDuration = crossfadeSec;
            }

            // SFML 3: constructor ile aç, hata olursa exception fırlatır
            try {
                m_currentMusic = std::make_unique<sf::Music>(filepath);
            } catch (...) {
                std::cerr << "AudioManager: muzik yuklenemedi -> " << filepath << '\n';
                m_currentMusic.reset();
                return;
            }

            // SFML 3'te setLoop() yok — loop için setLoopPoints kullanılır
            // Tüm dosyayı döngüye almak için: baştan sona setLoopPoints
            if (loop) {
                // getDuration() müziğin uzunluğunu verir; 0'dan sonuna kadar döngü
                // Not: play() çağrısından önce duration bilinmez, bu yüzden
                // alternatif olarak update()'te bitip bitmediğini kontrol ederiz.
                m_loopMusic = loop;
            }

            m_currentMusicPath = filepath;
            m_currentMusic->setVolume(crossfadeSec > 0.f
                                          ? 0.f
                                          : effectiveVolume(AudioBus::Music));
            m_currentMusic->play();
        }

        void stopMusic(float fadeDuration = 1.f) {
            if (!m_currentMusic) return;
            m_outgoingMusic = std::move(m_currentMusic);
            m_crossfadeTimer = 0.f;
            m_crossfadeDuration = fadeDuration;
        }

        void pauseMusic() { if (m_currentMusic) m_currentMusic->pause(); }

        void resumeMusic() {
            if (m_currentMusic)
                m_currentMusic->play();
            else if (!m_currentMusicPath.empty())
                playMusic(m_currentMusicPath, m_loopMusic, 0.f);
        }

        bool isMusicPlaying() const {
            return m_currentMusic &&
                   m_currentMusic->getStatus() == sf::Music::Status::Playing;
        }

        // ── Her frame çağrılmalı ──────────────────────────────────────────────────
        void update(float dt) {
            // Crossfade güncelle
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
                if (m_currentMusic)
                    m_currentMusic->setVolume(effectiveVolume(AudioBus::Music) * t);
            } else if (m_currentMusic) {
                m_currentMusic->setVolume(effectiveVolume(AudioBus::Music));

                // SFML 3'te setLoop() olmadığı için manuel loop
                if (m_loopMusic &&
                    m_currentMusic->getStatus() == sf::Music::Status::Stopped) {
                    m_currentMusic->play();
                }
            }
        }

        void stopAll() {
            m_voices.clear();
            if (m_currentMusic) m_currentMusic->stop();
            if (m_outgoingMusic) m_outgoingMusic->stop();
        }

    private:
        float effectiveVolume(AudioBus bus) const {
            float master = m_busVolumes[static_cast<int>(AudioBus::Master)];
            float local = m_busVolumes[static_cast<int>(bus)];
            return master * local / 100.f;
        }

        // Buffer map: id → PCM verisi
        std::unordered_map<std::string, sf::SoundBuffer> m_buffers;

        // Voice pool: liste, her eleman bir sf::Sound örneği
        // SFML 3'te sf::Sound default constructor yok — listeye emplace_back ile eklenir
        std::list<sf::Sound> m_voices;

        // Bus volümleri (Master, SFX, Music, UI)
        std::array<float, 4> m_busVolumes{100.f, 100.f, 100.f, 100.f};

        // Müzik
        std::unique_ptr<sf::Music> m_currentMusic;
        std::unique_ptr<sf::Music> m_outgoingMusic;
        std::string m_currentMusicPath;
        bool m_loopMusic{false};
        float m_crossfadeTimer{0.f};
        float m_crossfadeDuration{0.f};
    };
} // namespace engine
