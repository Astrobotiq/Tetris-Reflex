#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ParticleEffect.hpp  —  Tetris satır temizleme efektleri için basit sistem
//
// Motorun ECS yapısından bağımsızdır. Sadece TetrisScene içinde çağrılır.
// SFML 3 standartlarına (Vector2 parametreleri, std::uint8_t) uygundur.
// ─────────────────────────────────────────────────────────────────────────────

#include <SFML/Graphics.hpp>
#include <vector>
#include <random>
#include <cstdint>
#include <algorithm>

namespace tetris {

    struct Particle {
        sf::Vector2f position;
        sf::Vector2f velocity;
        sf::Color    color;
        float        lifetime;
        float        initialLifetime;
    };

    class ParticleEffect {
    public:
        // Belirtilen konuma rastgele yönlere saçılan partiküller fırlatır
        void emit(sf::Vector2f pos, sf::Color color, int count) {
            // C++ Mentör Notu: mt19937 performansı için static yapmak mantıklıdır
            // Yoksa her emit çağrısında RNG motoru yeniden inşa edilir.
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> angleDist(0.f, 3.14159f * 2.f);
            std::uniform_real_distribution<float> speedDist(80.f, 250.f);
            std::uniform_real_distribution<float> lifeDist(0.3f, 0.7f);

            for (int i = 0; i < count; ++i) {
                float angle = angleDist(rng);
                float speed = speedDist(rng);

                Particle p;
                p.position = pos;
                // Açı ve hızdan -> X, Y vektörüne (Trigonometri)
                p.velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
                p.color = color;
                p.initialLifetime = p.lifetime = lifeDist(rng);
                m_particles.push_back(p);
            }
        }

        void update(float dt) {
            for (auto& p : m_particles) {
                // Fizik integrasyonu
                p.position.x += p.velocity.x * dt;
                p.position.y += p.velocity.y * dt;
                p.lifetime -= dt;

                // Yaşlandıkça saydamlaşma efekti (Alpha fade)
                float ratio = std::max(0.f, p.lifetime / p.initialLifetime);
                p.color.a = static_cast<std::uint8_t>(255.f * ratio);
            }

            m_particles.erase(
                std::remove_if(m_particles.begin(), m_particles.end(),
                    [](const Particle& p) { return p.lifetime <= 0.f; }),
                m_particles.end()
            );
        }

        void draw(sf::RenderWindow& window) const {
            // SFML 3: FloatRect yerine boyut vektörü istiyor
            sf::RectangleShape rect({ 6.f, 6.f });
            for (const auto& p : m_particles) {
                rect.setPosition(p.position);
                rect.setFillColor(p.color);
                window.draw(rect);
            }
        }

        void clear() {
            m_particles.clear();
        }

    private:
        std::vector<Particle> m_particles;
    };

} // namespace tetris