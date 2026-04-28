#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// TetrisScene.hpp  —  Engine SceneManager'a bağlı Tetris sahnesi
//
// Pencere düzeni (600 × 740):
//   ┌─────────────┬────────────┐
//   │  BOARD      │  Sağ panel │  ← 0..575 y
//   │  320×576    │  skor/next │
//   ├─────────────┴────────────┤
//   │  SEÇİM ÇUBUĞU  120px    │  ← 576..696 y
//   └──────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────

#include "../SceneManager.hpp"
#include "GameState.hpp"
#include "BoardRenderer.hpp"
#include "SelectionBar.hpp"
#include "ParticleEffect.hpp"
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>

namespace tetris {

	inline constexpr unsigned WIN_W = 600;
	inline constexpr unsigned WIN_H = 740;

	// Board tahtanın ekrandaki konumu
	inline constexpr float BOARD_X = 30.f;
	inline constexpr float BOARD_Y = 20.f;
	inline constexpr float BOARD_PIXEL_W = BOARD_COLS * CELL_SIZE;  // 320
	inline constexpr float BOARD_PIXEL_H = BOARD_ROWS * CELL_SIZE;  // 576

	inline constexpr float BAR_Y = BOARD_Y + BOARD_PIXEL_H + 4.f;

	class TetrisScene : public engine::Scene {
	public:
		explicit TetrisScene(sf::RenderWindow& window)
			: m_window(window)
			, m_gameState(static_cast<std::uint32_t>(std::time(nullptr)))
			, m_boardRenderer({ BOARD_X, BOARD_Y })
			, m_selectionBar({ 0.f, BAR_Y }, float(WIN_W))
		{
			// Callbacks
			m_gameState.onLinesCleared = [this](int /*n*/, std::vector<int> rows) {
				m_flashRows = rows;
				m_flashTimer = 0.f;
				m_flashDuration = 0.35f;
				addLineClearParticles(rows);
				};
			m_gameState.onGameOver = [this]() { m_showGameOver = true; };
			m_buildVersion = std::string("Build: ") + __DATE__ + " " + __TIME__;
		}

		void onEnter() override {
			// Font yükle (yoksa çalışmaya devam et)
			if (m_font.openFromFile("assets/font.ttf") ||
				m_font.openFromFile("assets/fonts/arial.ttf"))
			{
				m_selectionBar.setFont(m_font);
				m_fontLoaded = true;
			}
			m_particles.reserve(512);
		}

		void update(float dt, engine::InputManager& input) override {
			if (m_gameState.gameOver) {
				// R tuşu: yeniden başlat
				if (input.isPressed(sf::Keyboard::Key::R)) restartGame();
				return;
			}

			if (input.isPressed(sf::Keyboard::Key::P)) {
				m_gameState.paused = !m_gameState.paused;
			}

			if (input.isPressed(sf::Keyboard::Key::G)) {
				m_gameState.debugDisableGravity = !m_gameState.debugDisableGravity;
			}

			m_gameState.update(dt);
			updateFlash(dt);
			updateParticles(dt);
			updateDragPreview();
		}

		// SFML eventleri dışarıdan ilet (Application::run içinde pollEvent sonrası)
		void handleEvent(const sf::Event& event) {
			sf::Vector2f mouse = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window));
			if (const auto* e = event.getIf<sf::Event::MouseButtonPressed>()) {
				if (e->button == sf::Mouse::Button::Left)  m_selectionBar.onMousePress(mouse, m_gameState);
				if (e->button == sf::Mouse::Button::Right) m_selectionBar.onRightClick(mouse, m_gameState);
			}
			if (event.is<sf::Event::MouseMoved>()) m_selectionBar.onMouseMove(mouse);
			if (const auto* e = event.getIf<sf::Event::MouseButtonReleased>()) {
				if (e->button == sf::Mouse::Button::Left) {
					auto result = m_selectionBar.onMouseRelease(mouse, BOARD_X, BOARD_Y, BOARD_Y + BOARD_PIXEL_H, m_gameState);
					if (result.success) addPlaceParticles(mouse);
					m_boardRenderer.setDragPreview(std::nullopt, true);
				}
			}
		}

		void render(sf::RenderWindow& window, float /*alpha*/) override {
			// Arkaplan
			window.clear(sf::Color(10, 10, 20));

			// Tahta
			m_boardRenderer.setFlashRows(m_flashRows,
				m_flashDuration > 0 ? m_flashTimer / m_flashDuration : 0.f);
			m_boardRenderer.render(window, m_gameState.board,
				m_gameState.fallingPiece, true);

			// Parçacıklar
			renderParticles(window);

			// Seçim çubuğu
			m_selectionBar.render(window, m_gameState);

			// Sağ panel
			renderSidePanel(window);

			// Durum overlay'leri
			if (m_gameState.paused)  renderOverlay(window, "DURDURULDU", "P tusu: devam");
			if (m_showGameOver)      renderOverlay(window, "OYUN BITTI",
				"Skor: " + std::to_string(m_gameState.score) +
				"   R: Yeniden");
		}

	private:
		// ── Sürükleme önizlemesi güncelle ────────────────────────────────────────
		void updateDragPreview() {
			if (!m_selectionBar.isDragging()) {
				m_boardRenderer.setDragPreview(std::nullopt, true);
				return;
			}

			const auto& drag = m_selectionBar.dragState();
			sf::Vector2f mouse = drag.mousePos;

			sf::FloatRect br = m_boardRenderer.boardRect();
			float padding = CELL_SIZE * 2.f;
			br.position.x -= padding;
			br.size.x += padding * 2.f;

			if (!br.contains(mouse)) {
				m_boardRenderer.setDragPreview(std::nullopt, true);
				return;
			}

			int col = m_boardRenderer.screenXToCol(mouse.x);
			auto [valid, placed] = m_gameState.board.findDropPosition(drag.piece, col);
			m_boardRenderer.setDragPreview(placed, valid);
		}

		// ── Flash animasyonu ──────────────────────────────────────────────────────
		void updateFlash(float dt) {
			if (m_flashDuration <= 0.f) return;
			m_flashTimer += dt;
			if (m_flashTimer >= m_flashDuration) {
				m_flashTimer = 0.f;
				m_flashDuration = 0.f;
				m_flashRows.clear();
				m_boardRenderer.setFlashRows({}, 0.f);
			}
		}

		// ── Basit parçacık sistemi ────────────────────────────────────────────────
		struct SimpleParticle {
			sf::Vector2f pos, vel;
			sf::Color color;
			float life, maxLife, size;
		};

		void addLineClearParticles(const std::vector<int>& rows) {
			for (int r : rows) {
				for (int c = 0; c < BOARD_COLS; ++c) {
					for (int k = 0; k < 4; ++k) {
						SimpleParticle p;
						p.pos = { BOARD_X + c * CELL_SIZE + CELL_SIZE * 0.5f,
								   BOARD_Y + r * CELL_SIZE + CELL_SIZE * 0.5f };
						float angle = (rand() % 360) * 3.14159f / 180.f;
						float spd = 60.f + rand() % 120;
						p.vel = { std::cos(angle) * spd, std::sin(angle) * spd };
						p.color = sf::Color(200 + rand() % 55, 200 + rand() % 55, 100 + rand() % 155, 255);
						p.maxLife = p.life = 0.4f + (rand() % 30) / 100.f;
						p.size = 3.f + rand() % 4;
						m_particles.push_back(p);
					}
				}
			}
		}

		void addPlaceParticles(sf::Vector2f pos) {
			for (int k = 0; k < 8; ++k) {
				SimpleParticle p;
				p.pos = pos;
				float angle = (rand() % 360) * 3.14159f / 180.f;
				float spd = 40.f + rand() % 80;
				p.vel = { std::cos(angle) * spd, std::sin(angle) * spd };
				p.color = sf::Color(100 + rand() % 155, 200, 255, 220);
				p.maxLife = p.life = 0.25f + (rand() % 20) / 100.f;
				p.size = 4.f;
				m_particles.push_back(p);
			}
		}

		void updateParticles(float dt) {
			for (auto& p : m_particles) {
				p.pos += p.vel * dt;
				p.vel.y += 200.f * dt; // Yerçekimi
				p.life -= dt;
			}
			m_particles.erase(
				std::remove_if(m_particles.begin(), m_particles.end(),
					[](const SimpleParticle& p) { return p.life <= 0.f; }),
				m_particles.end());
		}

		void renderParticles(sf::RenderWindow& window) {
			sf::CircleShape circle;
			for (const auto& p : m_particles) {
				float t = p.life / p.maxLife;
				sf::Color c = p.color;
				c.a = static_cast<std::uint8_t>(t * 255);
				circle.setRadius(p.size * t);
				circle.setFillColor(c);
				circle.setOrigin({ circle.getRadius(), circle.getRadius() });
				circle.setPosition(p.pos);
				window.draw(circle);
			}
		}

		// ── Sağ panel: next parçalar, skor ───────────────────────────────────────
		void renderSidePanel(sf::RenderWindow& window) {
			float panelX = BOARD_X + BOARD_PIXEL_W + 20.f;
			float panelY = BOARD_Y;

			// Arkaplan
			sf::RectangleShape panel({ float(WIN_W) - panelX - 10.f, BOARD_PIXEL_H });
			panel.setPosition({ panelX, panelY });
			panel.setFillColor(sf::Color(18, 18, 30));
			panel.setOutlineColor(sf::Color(60, 60, 100));
			panel.setOutlineThickness(1.5f);
			window.draw(panel);

			if (!m_fontLoaded) return;

			// Başlık
			drawText(window, "SONRAKI", { panelX + 10.f, panelY + 10.f }, 14,
				sf::Color(160, 160, 220));

			// Sonraki 3 parça
			for (int i = 0; i < 3; ++i) {
				drawMiniPiece(window, m_gameState.nextPieces[i],
					{ panelX + 20.f, panelY + 40.f + i * 70.f }, 18.f);
			}

			// Skor bilgileri
			float infoY = panelY + 260.f;
			drawText(window, "SKOR", { panelX + 10.f, infoY }, 13, sf::Color(140, 140, 200));
			drawText(window, std::to_string(m_gameState.score),
				{ panelX + 10.f, infoY + 18.f }, 18, sf::Color(255, 255, 100));
			drawText(window, "SEVIYE", { panelX + 10.f, infoY + 50.f }, 13, sf::Color(140, 140, 200));
			drawText(window, std::to_string(m_gameState.level),
				{ panelX + 10.f, infoY + 68.f }, 18, sf::Color(100, 255, 180));
			drawText(window, "SATIRLAR", { panelX + 10.f, infoY + 100.f }, 13, sf::Color(140, 140, 200));
			drawText(window, std::to_string(m_gameState.linesCleared),
				{ panelX + 10.f, infoY + 118.f }, 18, sf::Color(180, 220, 255));

			// Kontroller
			float ctrlY = panelY + BOARD_PIXEL_H - 120.f;
			drawText(window, "KONTROLLER", { panelX + 10.f, ctrlY }, 12, sf::Color(100, 100, 160));
			drawText(window, "Surukle: Yerlestir", { panelX + 10.f, ctrlY + 20.f }, 11, sf::Color(140, 140, 180));
			drawText(window, "Sag tik: Dondur", { panelX + 10.f, ctrlY + 36.f }, 11, sf::Color(140, 140, 180));
			drawText(window, "P: Durdur / Devam", { panelX + 10.f, ctrlY + 52.f }, 11, sf::Color(140, 140, 180));
			drawText(window, "R: Yeniden Baslat", { panelX + 10.f, ctrlY + 68.f }, 11, sf::Color(140, 140, 180));

			drawText(window, "G: Yercekimi (Test)", { panelX + 10.f, ctrlY + 84.f }, 11, sf::Color(140, 140, 180));

			// Debug uyarı metni
			if (m_gameState.debugDisableGravity) {
				drawText(window, "[YERCEKIMI KAPALI]", { panelX + 10.f, ctrlY + 105.f }, 12, sf::Color(255, 100, 100));
			}

			drawText(window, m_buildVersion, { panelX + 10.f, ctrlY + 125.f }, 10, sf::Color(100, 100, 120));
		}

		void drawMiniPiece(sf::RenderWindow& window, const Piece& piece,
			sf::Vector2f origin, float cellSize) {
			sf::RectangleShape cell({ cellSize - 2.f, cellSize - 2.f });
			cell.setFillColor(piece.color());
			sf::Color bright = piece.color();
			bright.r = static_cast<std::uint8_t>(std::min(255, static_cast<int>(bright.r) + 60));
			bright.g = static_cast<std::uint8_t>(std::min(255, static_cast<int>(bright.g) + 60));
			bright.b = static_cast<std::uint8_t>(std::min(255, static_cast<int>(bright.b) + 60));
			cell.setOutlineColor(bright);
			cell.setOutlineThickness(-1.5f);

			// Bounding box
			int minR = 4, maxR = -1, minC = 4, maxC = -1;
			for (auto [r, c] : piece.localCells()) {
				minR = std::min(minR, r); maxR = std::max(maxR, r);
				minC = std::min(minC, c); maxC = std::max(maxC, c);
			}
			if (maxR < 0) return;
			for (auto [r, c] : piece.localCells()) {
				cell.setPosition({
					origin.x + static_cast<float>(c - minC) * cellSize + 1.f,
					origin.y + static_cast<float>(r - minR) * cellSize + 1.f
					});
				window.draw(cell);
			}
		}

		void drawText(sf::RenderWindow& window, const std::string& str,
			sf::Vector2f pos, unsigned size,
			sf::Color color = sf::Color(220, 220, 255)) {
			sf::Text text(m_font);
			text.setString(sf::String::fromUtf8(str.begin(), str.end()));
			text.setCharacterSize(size);
			text.setFillColor(color);
			text.setPosition(pos);
			window.draw(text);
		}

		void renderOverlay(sf::RenderWindow& window,
			const std::string& title, const std::string& sub) {
			// Yarı şeffaf karartma
			sf::RectangleShape dim({ float(WIN_W), float(WIN_H) });
			dim.setFillColor(sf::Color(0, 0, 0, 160));
			window.draw(dim);

			if (!m_fontLoaded) return;

			sf::Text t(m_font);
			t.setString(sf::String::fromUtf8(title.begin(), title.end()));
			t.setCharacterSize(42);
			t.setFillColor(sf::Color(255, 220, 60));
			t.setStyle(sf::Text::Bold);
			auto bounds = t.getLocalBounds();
			t.setPosition({ (WIN_W - bounds.size.x) * 0.5f, WIN_H * 0.35f });
			window.draw(t);

			sf::Text s(m_font);
			s.setString(sf::String::fromUtf8(sub.begin(), sub.end()));
			s.setCharacterSize(20);
			s.setFillColor(sf::Color(200, 200, 255));
			auto sb = s.getLocalBounds();
			s.setPosition({ (WIN_W - sb.size.x) * 0.5f, WIN_H * 0.35f + 60.f });
			window.draw(s);
		}

		void restartGame() {
			m_gameState = GameState(static_cast<std::uint32_t>(std::time(nullptr)));
			m_gameState.onLinesCleared = [this](int /*n*/, std::vector<int> rows) {
				m_flashRows = rows; m_flashTimer = 0.f; m_flashDuration = 0.35f;
				addLineClearParticles(rows);
				};
			m_gameState.onGameOver = [this]() { m_showGameOver = true; };
			m_showGameOver = false;
			m_particles.clear();
			m_flashRows.clear();
			m_flashTimer = m_flashDuration = 0.f;
		}

		sf::RenderWindow& m_window;
		GameState         m_gameState;
		BoardRenderer     m_boardRenderer;
		SelectionBar      m_selectionBar;

		sf::Font          m_font;
		bool              m_fontLoaded{ false };
		bool              m_showGameOver{ false };

		// Flash animasyonu
		std::vector<int>  m_flashRows;
		float             m_flashTimer{ 0.f };
		float             m_flashDuration{ 0.f };

		// Parçacıklar
		std::vector<SimpleParticle> m_particles;

		std::string m_buildVersion;
	};

} // namespace tetris