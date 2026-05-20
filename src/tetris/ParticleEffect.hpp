#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <random>
#include <cmath>
#include <cstdint>

namespace tetris {

    struct Particle {
        sf::Vector2f position;
        sf::Vector2f velocity;
        sf::Color    colorStart;
        sf::Color    colorEnd;
        float        lifetime{0.f};
        float        initialLifetime{0.f};
        float        sizeStart{6.f};
        float        sizeEnd{0.f};
        float        rotation{0.f};
        float        angularVelocity{0.f};
        float        drag{0.f};     // Havada yavaşlama çarpanı
        float        gravity{0.f};  // Aşağı çekim kuvveti
        bool         alive{false};
    };

    class ParticleEffect {
    public:
        // Object Pool kapasitesi (Oyun boyunca asla RAM'de artış olmaz)
        static constexpr size_t MAX_PARTICLES = 1000;

        ParticleEffect() {
            m_rng.seed(std::random_device{}());
        }

        // 1. Profil: Satır Silinme (Debris) - Yerçekimi var, blok parçaları gibi havada dönerler
        void emitLineClear(sf::Vector2f pos, sf::Color color, int count) {
            std::uniform_real_distribution<float> angleDist(0.f, 3.14159f * 2.f);
            std::uniform_real_distribution<float> speedDist(100.f, 300.f);
            std::uniform_real_distribution<float> lifeDist(0.4f, 0.8f);
            std::uniform_real_distribution<float> rotDist(-500.f, 500.f);

            for (int i = 0; i < count; ++i) {
                if (Particle* p = getDeadParticle()) {
                    float angle = angleDist(m_rng);
                    float speed = speedDist(m_rng);

                    p->position = pos;
                    p->velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
                    p->colorStart = color;
                    p->colorEnd = sf::Color(color.r, color.g, color.b, 0); // Saydamlaşarak yok ol
                    p->initialLifetime = p->lifetime = lifeDist(m_rng);
                    p->sizeStart = 6.f;
                    p->sizeEnd = 2.f; // Küçülerek yok ol
                    p->rotation = angleDist(m_rng) * 180.f / 3.14159f;
                    p->angularVelocity = rotDist(m_rng);
                    p->drag = 1.5f; // Hafif sürtünme
                    p->gravity = 600.f; // Güçlü yerçekimi (aşağı dökülme)
                    p->alive = true;
                }
            }
        }

        // 2. Profil: Güç Patlaması (Explosion) - Yerçekimi yok, yüksek sürtünme, çok parlak
        void emitExplosion(sf::Vector2f pos, sf::Color color, int count) {
            std::uniform_real_distribution<float> angleDist(0.f, 3.14159f * 2.f);
            std::uniform_real_distribution<float> speedDist(300.f, 600.f); // Çok hızlı çıkar
            std::uniform_real_distribution<float> lifeDist(0.3f, 0.6f);

            for (int i = 0; i < count; ++i) {
                if (Particle* p = getDeadParticle()) {
                    float angle = angleDist(m_rng);
                    float speed = speedDist(m_rng);

                    p->position = pos;
                    p->velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
                    p->colorStart = color;
                    p->colorEnd = sf::Color(color.r, color.g, color.b, 0);
                    p->initialLifetime = p->lifetime = lifeDist(m_rng);
                    p->sizeStart = 8.f;
                    p->sizeEnd = 0.f;
                    p->rotation = 0.f;
                    p->angularVelocity = 0.f;
                    p->drag = 5.f; // Yüksek sürtünme (hızlıca yavaşlar, patlama hissi verir)
                    p->gravity = 0.f; // Yerçekimi yok (merkezde kalır)
                    p->alive = true;
                }
            }
        }

        // 3. Profil: Parça Yerleştirme (Dust/Sparks) - Yukarı doğru seken ufak tozlar
        void emitPlace(sf::Vector2f pos, sf::Color color, int count) {
            // Sadece yukarı doğru bir koni şeklinde fırlat
            std::uniform_real_distribution<float> angleDist(3.14159f, 3.14159f * 2.f);
            std::uniform_real_distribution<float> speedDist(50.f, 150.f);
            std::uniform_real_distribution<float> lifeDist(0.2f, 0.4f);

            for (int i = 0; i < count; ++i) {
                if (Particle* p = getDeadParticle()) {
                    float angle = angleDist(m_rng);
                    float speed = speedDist(m_rng);

                    p->position = pos;
                    p->velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
                    p->colorStart = color;
                    p->colorEnd = sf::Color(color.r, color.g, color.b, 0);
                    p->initialLifetime = p->lifetime = lifeDist(m_rng);
                    p->sizeStart = 4.f;
                    p->sizeEnd = 0.f;
                    p->rotation = 0.f;
                    p->angularVelocity = 0.f;
                    p->drag = 2.f;
                    p->gravity = 300.f; // Hafif aşağı düşüş
                    p->alive = true;
                }
            }
        }

        void update(float dt) {
            for (auto& p : m_pool) {
                if (!p.alive) continue;

                p.lifetime -= dt;
                if (p.lifetime <= 0.f) {
                    p.alive = false;
                    continue;
                }

                // Fizik: Sürtünme (Drag)
                p.velocity.x -= p.velocity.x * p.drag * dt;
                p.velocity.y -= p.velocity.y * p.drag * dt;

                // Fizik: Yerçekimi (Gravity)
                p.velocity.y += p.gravity * dt;

                // Konum ve Rotasyon entegrasyonu
                p.position.x += p.velocity.x * dt;
                p.position.y += p.velocity.y * dt;
                p.rotation += p.angularVelocity * dt;
            }
        }

        void draw(sf::RenderWindow& window) const {
            sf::RectangleShape rect(sf::Vector2f{6.f, 6.f});

            for (const auto& p : m_pool) {
                if (!p.alive) continue;

                float ratio = 1.f - (p.lifetime / p.initialLifetime); // 0.0 (doğum) -> 1.0 (ölüm)

                // Renk Interpolasyonu
                sf::Color c;
                c.r = static_cast<std::uint8_t>(p.colorStart.r + (p.colorEnd.r - p.colorStart.r) * ratio);
                c.g = static_cast<std::uint8_t>(p.colorStart.g + (p.colorEnd.g - p.colorStart.g) * ratio);
                c.b = static_cast<std::uint8_t>(p.colorStart.b + (p.colorEnd.b - p.colorStart.b) * ratio);
                c.a = static_cast<std::uint8_t>(p.colorStart.a + (p.colorEnd.a - p.colorStart.a) * ratio);

                // Boyut Interpolasyonu
                float currentSize = p.sizeStart + (p.sizeEnd - p.sizeStart) * ratio;

                rect.setSize(sf::Vector2f{currentSize, currentSize});
                rect.setOrigin(sf::Vector2f{currentSize * 0.5f, currentSize * 0.5f}); // Merkezden dön/büyü
                rect.setPosition(p.position);

                // SFML 3: Rotation artık sf::Angle bekler
                rect.setRotation(sf::degrees(p.rotation));
                rect.setFillColor(c);

                // Parlak neon efekti için Additive Blending kullanıyoruz
                window.draw(rect, sf::BlendAdd);
            }
        }

        void clear() {
            for (auto& p : m_pool) p.alive = false;
        }

    private:
        std::array<Particle, MAX_PARTICLES> m_pool;
        std::mt19937 m_rng;
        size_t m_lastUsedIndex = 0; // Pool aramayı hızlandırmak için Ring Buffer tarzı indeks

        Particle* getDeadParticle() {
            for (size_t i = 0; i < MAX_PARTICLES; ++i) {
                // Son kullanılan indexten başlayarak ileriye doğru tara
                size_t index = (m_lastUsedIndex + i) % MAX_PARTICLES;
                if (!m_pool[index].alive) {
                    m_lastUsedIndex = index;
                    return &m_pool[index];
                }
            }
            return nullptr; // Havuz doluysa partikülü es geç (oyun çökmez)
        }
    };

} // namespace tetris