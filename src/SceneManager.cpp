#include "SceneManager.hpp"
#include <cassert>
#include <SFML/Graphics/RectangleShape.hpp>
#include <algorithm>

namespace engine {

	void SceneManager::push(std::unique_ptr<Scene> scene) {
		if (m_stack.empty()) {
			m_pendingScene = std::move(scene);
			m_transAction = TransitionAction::Push;
			performTransitionAction();
			m_transState = TransitionState::FadeIn;
			m_transTimer = 0.f;
			return;
		}
		if (m_transState != TransitionState::None) return;
		m_transState = TransitionState::FadeOut;
		m_transTimer = 0.f;
		m_transAction = TransitionAction::Push;
		m_pendingScene = std::move(scene);
	}

	void SceneManager::pop() {
		if (m_stack.empty() || m_transState != TransitionState::None) return;
		m_transState = TransitionState::FadeOut;
		m_transTimer = 0.f;
		m_transAction = TransitionAction::Pop;
		m_pendingScene = nullptr;
	}

	void SceneManager::replace(std::unique_ptr<Scene> scene) {
		if (m_stack.empty()) {
			m_pendingScene = std::move(scene);
			m_transAction = TransitionAction::Replace;
			performTransitionAction();
			m_transState = TransitionState::FadeIn;
			m_transTimer = 0.f;
			return;
		}
		if (m_transState != TransitionState::None) return;
		m_transState = TransitionState::FadeOut;
		m_transTimer = 0.f;
		m_transAction = TransitionAction::Replace;
		m_pendingScene = std::move(scene);
	}

	void SceneManager::performTransitionAction() {
		if (m_transAction == TransitionAction::Push) {
			if (!m_stack.empty()) {
				m_stack.back()->onSuspend();
			}
			m_pendingScene->m_sceneManager = this;
			m_pendingScene->onEnter();
			m_stack.push_back(std::move(m_pendingScene));
		} else if (m_transAction == TransitionAction::Pop) {
			assert(!m_stack.empty() && "Popping from an empty SceneManager stack.");
			m_stack.back()->onExit();
			m_stack.pop_back();
			if (!m_stack.empty()) {
				m_stack.back()->onResume();
			}
		} else if (m_transAction == TransitionAction::Replace) {
			if (!m_stack.empty()) {
				m_stack.back()->onExit();
				m_stack.pop_back();
			}
			m_pendingScene->m_sceneManager = this;
			m_pendingScene->onEnter();
			m_stack.push_back(std::move(m_pendingScene));
		}
	}

	void SceneManager::handleEvent(const sf::Event& event) {
		// Eventleri geçiş sırasında yutmak daha kararlıdır
		if (m_transState != TransitionState::None) return;

		if (!m_stack.empty()) {
			m_stack.back()->handleEvent(event);
		}
	}

	void SceneManager::update(float dt, InputManager& input) {
		if (m_transState == TransitionState::FadeOut) {
			m_transTimer += dt;
			if (m_transTimer >= m_transDuration) {
				performTransitionAction();
				m_transState = TransitionState::FadeIn;
				m_transTimer = 0.f;
			}
		} else if (m_transState == TransitionState::FadeIn) {
			m_transTimer += dt;
			if (m_transTimer >= m_transDuration) {
				m_transState = TransitionState::None;
			}
		}

		if (!m_stack.empty()) {
			m_stack.back()->update(dt, input);
		}
	}

	void SceneManager::render(sf::RenderWindow& window, float alpha) {
		if (!m_stack.empty()) {
			m_stack.back()->render(window, alpha);
		}

		// Siyah ekran geçiş overlay'i çiz
		if (m_transState != TransitionState::None) {
			float progress = m_transTimer / m_transDuration;
			float fadeAlpha = (m_transState == TransitionState::FadeOut) ? progress : (1.f - progress);

			sf::RectangleShape fadeRect(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
			sf::View oldView = window.getView();
			window.setView(window.getDefaultView());

			fadeRect.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(std::clamp(fadeAlpha, 0.f, 1.f) * 255)));
			window.draw(fadeRect);

			window.setView(oldView);
		}
	}

} // namespace engine