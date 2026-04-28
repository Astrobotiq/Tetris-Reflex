#pragma once

#include <algorithm>
#include <string>
#include <optional> // std::optional için gerekli
#include <SFML/Graphics.hpp>

namespace engine {

	struct TransformComponent {
		sf::Vector2f position{ 0.f, 0.f };
		float        rotation{ 0.f };      // Derece cinsinden
		sf::Vector2f scale{ 1.f, 1.f };

		TransformComponent() = default;
		TransformComponent(sf::Vector2f pos, float rot = 0.f, sf::Vector2f scl = { 1.f, 1.f })
			: position(pos), rotation(rot), scale(scl) {
		}
	};

	struct SpriteComponent {
		// SFML 3'te Sprite default ctor'a sahip olmadığı için optional kullanıyoruz.
		// Bu aynı zamanda 'boş sprite' durumunu yönetmemizi sağlar.
		std::optional<sf::Sprite> sprite;
		bool visible{ true };

		SpriteComponent() = default; // std::optional sayesinde artık bu mümkün.

		explicit SpriteComponent(const sf::Texture& texture, bool vis = true)
			: visible(vis)
		{
			sprite.emplace(texture); // Sprite'ı texture ile burada oluşturuyoruz.

			// SFML 3: width/height -> size.x/size.y
			auto bounds = sprite->getLocalBounds();
			sprite->setOrigin({ bounds.size.x * 0.5f, bounds.size.y * 0.5f });
		}
	};

	// ─── VelocityComponent ────────────────────────────────────────────────────────
	struct VelocityComponent {
		sf::Vector2f linear{ 0.f, 0.f }; // Pixels per second
		float        angular{ 0.f };       // Degrees per second

		VelocityComponent() = default;
		VelocityComponent(sf::Vector2f lin, float ang = 0.f)
			: linear(lin), angular(ang) {
		}
	};

	// ─── TagComponent ─────────────────────────────────────────────────────────────
	struct TagComponent {
		std::string tag;
		TagComponent() = default;
		explicit TagComponent(std::string t) : tag(std::move(t)) {}
	};

	// ─── HealthComponent ──────────────────────────────────────────────────────────
	// New in Tier 3: tracks HP and invincibility frames.
	//
	// WHY a separate component?
	//   Not every entity has health (platforms, particles, decorations don't).
	//   Adding it as a component means zero overhead for those entities.
	//   The PhysicsSystem and enemies only query entities that have it.
	//
	// NOTE: small helper methods here are acceptable (they're just convenience
	//   arithmetic on the data), not logic that belongs in a System.
	struct HealthComponent {
		float current{ 100.f };
		float maximum{ 100.f };
		float invincibleTimer{ 0.f };   // Seconds of invincibility remaining after a hit.
		bool  isDead{ false };

		HealthComponent() = default;
		explicit HealthComponent(float max) : current(max), maximum(max) {}

		float normalised() const { return (maximum > 0.f) ? (current / maximum) : 0.f; }

		// Returns true if damage was applied (not currently invincible).
		bool applyDamage(float amount, float invincibilityWindow = 0.5f) {
			if (invincibleTimer > 0.f) return false;
			current -= amount;
			invincibleTimer = invincibilityWindow;
			if (current <= 0.f) { current = 0.f; isDead = true; }
			return true;
		}

		void heal(float amount) {
			current = std::min(current + amount, maximum);
			isDead = false;
		}

		// Tick down the invincibility timer. Call from a System each fixed step.
		void update(float dt) {
			if (invincibleTimer > 0.f)
				invincibleTimer = std::max(0.f, invincibleTimer - dt);
		}
	};

} // namespace engine
