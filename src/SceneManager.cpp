#include "SceneManager.hpp"
#include <cassert>

namespace engine {

	void SceneManager::push(std::unique_ptr<Scene> scene) {
		// Suspend the current top before pushing the new scene.
		if (!m_stack.empty()) {
			m_stack.back()->onSuspend();
		}

		// Give the scene a back-pointer so it can trigger transitions.
		scene->m_sceneManager = this;
		scene->onEnter();

		// std::move transfers ownership into the vector.
		// After the move, 'scene' is null — the vector owns the object.
		m_stack.push_back(std::move(scene));
	}

	void SceneManager::pop() {
		assert(!m_stack.empty() && "Popping from an empty SceneManager stack.");

		m_stack.back()->onExit();
		m_stack.pop_back(); // unique_ptr destructor runs here — memory freed automatically.

		if (!m_stack.empty()) {
			m_stack.back()->onResume();
		}
	}

	void SceneManager::replace(std::unique_ptr<Scene> scene) {
		if (!m_stack.empty()) {
			m_stack.back()->onExit();
			m_stack.pop_back();
		}

		scene->m_sceneManager = this;
		scene->onEnter();
		m_stack.push_back(std::move(scene));
	}

	// EKLENDİ: Application'dan gelen eventi en üstteki (aktif) sahneye yönlendir
	void SceneManager::handleEvent(const sf::Event& event) {
		if (!m_stack.empty()) {
			m_stack.back()->handleEvent(event);
		}
	}

	void SceneManager::update(float dt, InputManager& input) {
		if (!m_stack.empty()) {
			m_stack.back()->update(dt, input);
		}
	}

	void SceneManager::render(sf::RenderWindow& window, float alpha) {
		if (!m_stack.empty()) {
			m_stack.back()->render(window, alpha);
		}
	}

} // namespace engine