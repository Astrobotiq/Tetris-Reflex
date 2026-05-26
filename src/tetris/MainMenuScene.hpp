//
// Created by e-r-e on 5/20/2026.
//

#ifndef SIMPLEENGINE2D_MAINMENUSCENE_HPP
#define SIMPLEENGINE2D_MAINMENUSCENE_HPP

#endif //SIMPLEENGINE2D_MAINMENUSCENE_HPP
#pragma once
#include "../SceneManager.hpp"
#include "tetris/AppSettings.hpp"
#include "tetris/ScoreManager.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <functional>
#include <string>

namespace tetris {

    class MainMenuScene : public engine::Scene {
    public:
        MainMenuScene(sf::RenderWindow& win, AppSettings* appSettings);

        void onEnter() override;
        void onExit() override;
        void handleEvent(const sf::Event& event) override;
        void update(float dt, engine::InputManager& input) override;
        void render(sf::RenderWindow& window, float alpha) override;

    private:
        sf::RenderWindow& window;
        AppSettings*      settings;

        sf::Font font;
        bool fontLoaded{false};

        sf::FloatRect btnNewGame;
        sf::FloatRect btnHighScores;
        sf::FloatRect btnQuit;
        sf::FloatRect btnSfx;
        sf::FloatRect btnMusic;
        std::vector<sf::FloatRect> paletteBtns;

        int hoveredBtn{-1}; // 0: New Game, 1: High Scores, 2: Quit, 3: SFX, 4: Music, 5-8: Palettes
        std::vector<float> btnScales;

        bool showScoreTable{false};
        std::vector<ScoreManager::Entry> highScores;
        sf::FloatRect btnBack;
        bool backHovered{false};

        // UI Animation & Juice Variables
        float m_introTimer{0.f};
        float m_elapsedTime{0.f};
        float m_scoreTableTimer{0.f};
        sf::Vector2f m_mouseTilt{0.f, 0.f};
        int m_clickedBtn{-1};
        float m_clickTimer{0.f};

        struct MenuParticle {
            sf::Vector2f position;
            sf::Vector2f velocity;
            sf::Color color;
            float age{0.f};
            float lifetime{1.f};
            float size{4.f};
        };
        std::vector<MenuParticle> m_particles;

        void setupLayout();
        void renderScoreTable(sf::RenderWindow& window, const sf::Transform& transform);
    };

} // namespace tetris