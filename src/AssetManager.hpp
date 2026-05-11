#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>

namespace engine {
    class AssetManager {
    public:
        // C#'taki Dictionary<string, Font> kar��l���
        sf::Font &getFont(const std::string &path) {
            // Font daha �nce y�klendiyse direkt onu d�nd�r
            if (auto it = m_fonts.find(path); it != m_fonts.end()) {
                return it->second;
            }

            // Yoksa yeni bir font olu�tur, y�kle ve haritaya (map) kaydet
            sf::Font font;
            if (!font.openFromFile(path)) {
                // SFML 3'te fontlar openFromFile kullan�r
                std::cerr << "HATA: Font yuklenemedi -> " << path << '\n';
            }

            // std::move ile kopyalama yapmadan sahipli�i map'e devrediyoruz
            return m_fonts.emplace(path, std::move(font)).first->second;
        }

        sf::Texture &getTexture(const std::string &path) {
            if (auto it = m_textures.find(path); it != m_textures.end()) {
                return it->second;
            }

            sf::Texture texture;
            if (!texture.loadFromFile(path)) {
                std::cerr << "HATA: Doku yuklenemedi -> " << path << '\n';
            }

            return m_textures.emplace(path, std::move(texture)).first->second;
        }

        sf::SoundBuffer &getSound(const std::string &path) {
            if (auto it = m_sounds.find(path); it != m_sounds.end()) {
                return it->second;
            }
            sf::SoundBuffer buf;
            if (!buf.loadFromFile(path)) {
                std::cerr << "HATA: Ses yuklenemedi -> " << path << '\n';
            }
            return m_sounds.emplace(path, std::move(buf)).first->second;
        }

    private:
        std::unordered_map<std::string, sf::Font> m_fonts;
        std::unordered_map<std::string, sf::Texture> m_textures;
        std::unordered_map<std::string, sf::SoundBuffer> m_sounds;
    };
} // namespace engine
