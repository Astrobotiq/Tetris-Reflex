#include "GameLoop.hpp"
#include <SFML/Window/Event.hpp>
#include <algorithm> // std::min

namespace engine {

	GameLoop::GameLoop(sf::RenderWindow& window, float fixedTimestep)
		: m_window(window)
		, m_fixedTimestep(fixedTimestep)
	{
	}

	void GameLoop::run(const EventFn& onEvent, const UpdateFn& onUpdate, const RenderFn& onRender) {
		sf::Clock clock;

		// The accumulator holds "unprocessed" time between frames.
		// Think of it as a bucket: each frame pours frameTime seconds in;
		// we drain it in fixed-timestep increments.
		float accumulator = 0.f;

		while (m_window.isOpen()) {
			// ── 1. Measure elapsed time ──────────────────────────────────────────
			float frameTime = clock.restart().asSeconds();

			// Clamp to prevent the "spiral of death": if the game falls behind
			// (e.g., a debugger breakpoint), we cap the catch-up work.
			// Without this clamp, a 1-second hitch would queue ~60 update calls.
			constexpr float MAX_FRAME_TIME = 0.25f; // 250 ms max per frame
			frameTime = std::min(frameTime, MAX_FRAME_TIME);

			m_fps = (frameTime > 0.f) ? 1.f / frameTime : 0.f;


			// ── 2. Poll window events ────────────────────────────────────────────
			// SFML 3'te artık dışarıda 'sf::Event event;' tanımlamaya gerek yok.
			// pollEvent() bize doğrudan bir std::optional<sf::Event> döndürür.

			while (const std::optional<sf::Event> event = m_window.pollEvent()) {
				// event değişkeni burada bir 'optional' olduğu için -> operatörü ile erişiyoruz
				if (event->is<sf::Event::Closed>()) {
					m_window.close();
				}
				onEvent(*event);
			}

			// ── 3. Fixed-rate logic updates ──────────────────────────────────────
			accumulator += frameTime;

			while (accumulator >= m_fixedTimestep) {
				onUpdate(m_fixedTimestep);
				accumulator -= m_fixedTimestep;
			}

			// ── 4. Variable-rate rendering ───────────────────────────────────────
			// alpha ∈ [0, 1): how far we are between the last and next logic tick.
			// Pass this to the render callback for interpolation ("sub-stepping"):
			//   renderPos = previous + alpha * (current - previous)
			// This makes motion look buttery-smooth even when logic runs at 60 Hz
			// but the monitor refreshes at 144 Hz.
			const float alpha = accumulator / m_fixedTimestep;

			m_window.clear(sf::Color(30, 30, 40)); // Dark slate background
			onRender(alpha);
			m_window.display();
		}
	}

} // namespace engine
