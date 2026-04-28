#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SceneManager.hpp  —  Pushdown automaton for application states
//
// Games have distinct states: MainMenu → Gameplay → PauseMenu → Gameplay
//
// We model this as a STACK (pushdown automaton):
//   push(scene)   — enter a new state, suspending the one below
//   pop()         — return to the previous state
//   replace(scene)— swap the current state (no going back)
//
// WHY A STACK vs a simple "current state" pointer?
//   • Pause menus, inventory screens, dialogue boxes naturally overlay gameplay.
//   • The gameplay scene keeps its ECS state while the pause menu is on top.
//   • Popping the pause menu instantly resumes gameplay — no reload needed.
//
// Each Scene owns its own Registry (ECS world). This keeps scenes fully
// isolated from each other.
// ─────────────────────────────────────────────────────────────────────────────

#include "ECS.hpp"
#include "InputManager.hpp"
#include <memory>
#include <vector>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp> // EKLENDİ: Olayları tanıyabilmesi için

namespace engine {

	// Forward declaration — Scene references SceneManager for transitions.
	class SceneManager;

	// ─────────────────────────────────────────────────────────────────────────────
	// Scene — Abstract base class
	// Subclass this for each application state (MainMenuScene, GameScene, etc.)
	// ─────────────────────────────────────────────────────────────────────────────
	class Scene {
	public:
		// WHY virtual destructor?
		// When we delete a derived class through a base pointer (which SceneManager
		// does), without a virtual destructor the derived destructor won't be called
		// — causing resource leaks. Always add virtual ~Base() in polymorphic classes.
		virtual ~Scene() = default;

		// Called once when the scene is first pushed onto the stack.
		virtual void onEnter() {}

		// Called once when the scene is popped / replaced (cleanup here).
		virtual void onExit() {}

		// Called when another scene is pushed on top (e.g., pause menu appears).
		// Use this to stop sounds, animations, etc.
		virtual void onSuspend() {}

		// Called when the scene on top is popped and this scene resumes.
		virtual void onResume() {}

		// EKLENDİ: Alt sahnelerin (TetrisScene gibi) olayları alabilmesi için sanal metot
		virtual void handleEvent(const sf::Event& /*event*/) {}

		// Fixed-timestep logic update.
		virtual void update(float dt, InputManager& input) = 0;

		// Variable-rate render. alpha is the interpolation factor from GameLoop.
		virtual void render(sf::RenderWindow& window, float alpha) = 0;

		// Convenience access to this scene's ECS world.
		Registry& registry() { return m_registry; }

	protected:
		Registry      m_registry;
		SceneManager* m_sceneManager = nullptr; // Set by SceneManager on push.
		friend class SceneManager;
	};

	// ─────────────────────────────────────────────────────────────────────────────
	// SceneManager
	// ─────────────────────────────────────────────────────────────────────────────
	class SceneManager {
	public:
		// Push a new scene. Takes ownership via unique_ptr — the SceneManager
		// is the sole owner. unique_ptr is the right choice here because:
		//   • Each scene has exactly one owner (the manager)
		//   • Lifetime is deterministic (destroyed when popped)
		//   • No reference counting overhead (unlike shared_ptr)
		void push(std::unique_ptr<Scene> scene);

		// Pop the top scene (calls onExit, resumes the one below).
		void pop();

		// Replace the top scene without pushing (no resume for the old scene).
		void replace(std::unique_ptr<Scene> scene);

		// EKLENDİ: Olayları (Event) en üstteki sahneye ilet
		void handleEvent(const sf::Event& event);

		// Delegate update/render to the top scene.
		void update(float dt, InputManager& input);
		void render(sf::RenderWindow& window, float alpha);

		bool isEmpty() const { return m_stack.empty(); }
		Scene* top() const { return m_stack.empty() ? nullptr : m_stack.back().get(); }

	private:
		// vector of unique_ptrs: clear ownership semantics, easy stack operations.
		std::vector<std::unique_ptr<Scene>> m_stack;
	};

} // namespace engine