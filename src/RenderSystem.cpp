#include "RenderSystem.hpp"
#include "Components.hpp"

namespace engine {

	RenderSystem::RenderSystem(Registry& registry)
		: m_registry(registry)
	{
		// Pre-build the required signature so we don't rebuild it every frame.
		// This is a minor optimisation but illustrates the pattern.
		m_signature = registry.buildSignature<TransformComponent, SpriteComponent>();
	}

	void RenderSystem::render(sf::RenderWindow& window, float /*alpha*/) {
		auto entities = m_registry.view(m_signature);

		for (Entity entity : entities) {
			auto& transform = m_registry.getComponent<TransformComponent>(entity);
			auto& spriteComp = m_registry.getComponent<SpriteComponent>(entity);

			// Sprite yüklü deðilse veya görünmezse atla
			if (!spriteComp.visible || !spriteComp.sprite.has_value()) continue;

			// Optional içindeki gerçek sprite'a eriþim
			sf::Sprite& sprite = *spriteComp.sprite;

			sprite.setPosition(transform.position);

			// SFML 3: float -> sf::Angle dönüþümü þart!
			sprite.setRotation(sf::degrees(transform.rotation));

			sprite.setScale(transform.scale);

			window.draw(sprite);
		}
	}

} // namespace engine
