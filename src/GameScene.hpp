#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameScene.hpp  —  Tier 3 demo scene
//
// New systems demonstrated:
//   • TileMap rendering (single draw-call batched tiles)
//   • ParticleSystem (jump dust, death burst — object pool, zero alloc)
//   • AudioManager (voice pool, spatial SFX, music crossfade)
//   • StateMachine (patrolling enemy AI with Patrol→Chase→Return states)
//   • HealthComponent + damage/invincibility logic
//   • SceneSerializer (F5 = quicksave, F9 = quickload)
// ─────────────────────────────────────────────────────────────────────────────

#include "SceneManager.hpp"
#include "Components.hpp"
#include "AssetManager.hpp"
#include "Camera.hpp"
#include "EventBus.hpp"
#include "TileMap.hpp"
#include "AudioManager.hpp"
#include "StateMachine.hpp"
#include "SceneSerializer.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/PhysicsSystem.hpp"
#include "systems/AnimationSystem.hpp"
#include "systems/DebugRenderer.hpp"
#include "systems/ParticleSystem.hpp"
#include <cmath>
#include <memory>
#include <vector>

class GameScene : public engine::Scene {
public:
    explicit GameScene(sf::RenderWindow& window)
        : m_window(window)
        , m_camera(window)
        , m_audio(m_assets)
    {}

    void onEnter() override {
        // ── Register all component types used in this scene ──────────────────
        m_registry.registerComponent<engine::TransformComponent>();
        m_registry.registerComponent<engine::SpriteComponent>();
        m_registry.registerComponent<engine::VelocityComponent>();
        m_registry.registerComponent<engine::TagComponent>();
        m_registry.registerComponent<engine::ColliderComponent>();
        m_registry.registerComponent<engine::AnimatorComponent>();
        m_registry.registerComponent<engine::HealthComponent>();
        m_registry.registerComponent<engine::EmitterComponent>();
        m_registry.registerComponent<engine::TileMapComponent>();
        m_registry.registerComponent<engine::StateMachineComponent>();

        // ── Instantiate systems ──────────────────────────────────────────────
        m_renderSystem    = std::make_unique<engine::RenderSystem>(m_registry);
        m_physicsSystem   = std::make_unique<engine::PhysicsSystem>(m_registry);
        m_animationSystem = std::make_unique<engine::AnimationSystem>(m_registry);
        m_debugRenderer   = std::make_unique<engine::DebugRenderer>(m_registry);
        m_particleSystem  = std::make_unique<engine::ParticleSystem>(m_registry);
        m_tileMapSystem   = std::make_unique<engine::TileMapSystem>(m_registry, m_assets);
        m_stateMachineSystem = std::make_unique<engine::StateMachineSystem>(m_registry);

        m_physicsSystem->gravity = { 0.f, 600.f };

        // Wire physics → event bus → scene handlers.
        m_physicsSystem->onCollision([this](const engine::CollisionEvent& e) {
            m_eventBus.publish(e);
        });
        m_collisionHandle = m_eventBus.subscribe<engine::CollisionEvent>(
            [this](const engine::CollisionEvent& e) { onCollision(e); }
        );

        // ── Assets ───────────────────────────────────────────────────────────
        loadAssets();

        // ── World ────────────────────────────────────────────────────────────
        buildTileMap();
        spawnPlayer();
        spawnEnemies();

        // ── Camera ───────────────────────────────────────────────────────────
        m_camera.follow(m_registry, m_player);
        m_camera.setFollowSmoothing(0.07f);
        m_camera.setWorldBounds({ 0.f, 0.f, 1600.f, 600.f });

        m_debugRenderer->loadFont("assets/font.ttf");

        // Start background music.
        // m_audio.playMusic("assets/music/theme.ogg", true, 1.5f);
    }

    void onExit() override { /* RAII cleans everything up */ }

    void update(float dt, engine::InputManager& input) override {
        handleDebugKeys(input);
        handlePlayerInput(dt, input);

        // Tick health invincibility timers for all entities that have health.
        auto healthSig = m_registry.buildSignature<engine::HealthComponent>();
        for (engine::Entity e : m_registry.view(healthSig)) {
            m_registry.getComponent<engine::HealthComponent>(e).update(dt);
        }

        // Update all systems.
        m_stateMachineSystem->update(dt);
        m_physicsSystem->update(dt);
        m_animationSystem->update(dt);
        m_particleSystem->update(dt);
        m_camera.update(dt);
        m_audio.update(dt);
        m_debugRenderer->setFPS(1.f / std::max(dt, 0.0001f));

        // Cull dead enemies.
        pruneDeadEntities();
    }

    void render(sf::RenderWindow& window, float alpha) override {
        m_camera.applyTo(window);

        // Draw order: tilemap → sprites → particles → debug overlay.
        m_tileMapSystem->render(window);
        m_renderSystem->render(window, alpha);
        m_particleSystem->render(window);
        m_debugRenderer->render(window);

        window.setView(window.getDefaultView()); // Reset for HUD
        renderHUD(window);
    }

private:
    // ─────────────────────────────────────────────────────────────────────────
    // Asset loading
    // ─────────────────────────────────────────────────────────────────────────
    void loadAssets() {
        // Try loading real assets; fall back to generated placeholders silently.
        auto tryLoad = [&](const std::string& id, const std::string& path,
                           sf::Color fallback, unsigned w, unsigned h) {
            try { m_assets.loadTexture(id, path); }
            catch (...) {
                sf::Image img; img.create(w, h, fallback);
                m_fallbackTextures[id] = std::make_unique<sf::Texture>();
                m_fallbackTextures[id]->loadFromImage(img);
            }
        };

        tryLoad("player",  "assets/player.png",  sf::Color(100,200,255), 32, 48);
        tryLoad("enemy",   "assets/enemy.png",   sf::Color(255,100,100), 32, 48);
        tryLoad("tileset", "assets/tileset.png", sf::Color(120,100, 80), 256,256);
    }

    const sf::Texture& tex(const std::string& id) {
        if (m_fallbackTextures.count(id)) return *m_fallbackTextures[id];
        return m_assets.getTexture(id);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Tilemap construction
    // ─────────────────────────────────────────────────────────────────────────
    void buildTileMap() {
        engine::Entity mapEntity = m_registry.createEntity();
        auto& map = m_registry.addComponent<engine::TileMapComponent>(
            mapEntity, engine::TileMapComponent{});

        // Configure the tileset atlas.
        map.tileSet.tileW    = 32;
        map.tileSet.tileH    = 32;
        map.tileSet.columns  = 8; // 8 tiles wide on the atlas texture
        map.tileSet.firstGID = 1;
        map.tileSet.texturePath = "tileset";

        // Build a simple ground layer (50 × 19 tiles).
        constexpr int COLS = 50, ROWS = 19;
        auto& ground = map.addLayer("ground", COLS, ROWS);
        ground.isCollision = false; // TileMap collision is advisory only here;
                                    // we still use PhysicsSystem colliders for
                                    // precise resolution.

        // Fill the bottom rows with ground tiles and scatter platforms.
        // Tile ID 1 = first tile in atlas (brown ground).
        // Tile ID 2 = second tile (darker variant).
        // In a real game this data comes from Tiled (.tmx) or a level editor.
        for (int c = 0; c < COLS; ++c) {
            ground.set(c, 17, 1);  // Ground row
            ground.set(c, 18, 2);  // Underground row
        }
        // Floating platforms
        for (int c = 6;  c <= 11; ++c) ground.set(c, 12, 1);
        for (int c = 14; c <= 20; ++c) ground.set(c,  9, 1);
        for (int c = 24; c <= 29; ++c) ground.set(c, 13, 1);
        for (int c = 33; c <= 40; ++c) ground.set(c,  7, 1);
        for (int c = 44; c <= 49; ++c) ground.set(c, 11, 1);

        map.dirty = true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Player spawning
    // ─────────────────────────────────────────────────────────────────────────
    void spawnPlayer() {
        m_player = m_registry.createEntity();

        m_registry.addComponent<engine::TransformComponent>(m_player, {{ 100.f, 200.f }});
        m_registry.addComponent<engine::SpriteComponent>(m_player, engine::SpriteComponent{tex("player")});
        m_registry.addComponent<engine::VelocityComponent>(m_player, {});
        m_registry.addComponent<engine::ColliderComponent>(m_player,
            engine::ColliderComponent{{ 28.f, 44.f }, {}, false, false });
        m_registry.addComponent<engine::TagComponent>(m_player, engine::TagComponent{"Player"});
        m_registry.addComponent<engine::HealthComponent>(m_player, engine::HealthComponent{100.f});

        // Particle emitter for running dust (continuous, low rate).
        engine::EmitterComponent dustEmitter;
        dustEmitter.emissionRate   = 0.f; // Enabled/disabled from input logic.
        dustEmitter.speed          = 40.f;
        dustEmitter.speedSpread    = 20.f;
        dustEmitter.direction      = 90.f; // Downward
        dustEmitter.directionSpread = 30.f;
        dustEmitter.lifetime       = 0.4f;
        dustEmitter.lifetimeSpread = 0.1f;
        dustEmitter.sizeStart      = 5.f;
        dustEmitter.sizeEnd        = 0.f;
        dustEmitter.colorStart     = sf::Color(220, 210, 190, 180);
        dustEmitter.colorEnd       = sf::Color(220, 210, 190,   0);
        dustEmitter.gravityScale   = 0.f;
        m_dustEmitterEntity = m_player; // Dust emitter is on the player entity itself.
        m_registry.addComponent<engine::EmitterComponent>(m_player, dustEmitter);

        // Animation.
        engine::AnimatorComponent anim;
        anim.addClip(engine::AnimationClip::fromGrid("idle", 0, 1, 32, 48, 4.f));
        anim.addClip(engine::AnimationClip::fromGrid("walk", 0, 1, 32, 48, 10.f));
        anim.addClip(engine::AnimationClip::fromGrid("jump", 0, 1, 32, 48, 8.f, false));
        anim.play("idle");
        m_registry.addComponent<engine::AnimatorComponent>(m_player, std::move(anim));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Enemy spawning with State Machine AI
    // ─────────────────────────────────────────────────────────────────────────
    void spawnEnemies() {
        auto spawnEnemy = [&](float x, float patrolLeft, float patrolRight) {
            engine::Entity e = m_registry.createEntity();

            m_registry.addComponent<engine::TransformComponent>(e, {{ x, 200.f }});
            m_registry.addComponent<engine::SpriteComponent>(e, engine::SpriteComponent{tex("enemy")});
            m_registry.addComponent<engine::VelocityComponent>(e, {});
            m_registry.addComponent<engine::ColliderComponent>(e,
                engine::ColliderComponent{{ 28.f, 44.f }, {}, false, false });
            m_registry.addComponent<engine::TagComponent>(e, engine::TagComponent{"Enemy"});
            m_registry.addComponent<engine::HealthComponent>(e, engine::HealthComponent{50.f});

            // ── State machine: Patrol ↔ Chase ↔ Return ──────────────────────
            engine::StateMachineComponent sm;
            sm.set("patrolLeft",  patrolLeft);
            sm.set("patrolRight", patrolRight);
            sm.set("spawnX",      x);
            sm.set("dir",         1.f); // Current patrol direction

            // PATROL state — walk back and forth between patrol bounds.
            sm.addState(engine::State{
                "patrol",
                // onEnter
                [](engine::StateContext& ctx) {
                    ctx.blackboard->at("dir") = 1.f;
                },
                // onUpdate
                [](engine::StateContext& ctx) {
                    auto& tf  = ctx.registry->getComponent<engine::TransformComponent>(ctx.entity);
                    auto& vel = ctx.registry->getComponent<engine::VelocityComponent>(ctx.entity);
                    float dir = ctx.blackboard->at("dir");
                    constexpr float PATROL_SPEED = 80.f;
                    vel.linear.x = dir * PATROL_SPEED;

                    float left  = ctx.blackboard->at("patrolLeft");
                    float right = ctx.blackboard->at("patrolRight");
                    if (tf.position.x <= left)  ctx.blackboard->at("dir") =  1.f;
                    if (tf.position.x >= right) ctx.blackboard->at("dir") = -1.f;
                },
                nullptr // onExit
            });

            // CHASE state — move toward the player.
            sm.addState(engine::State{
                "chase",
                nullptr,
                [this](engine::StateContext& ctx) {
                    if (!ctx.registry->hasComponent<engine::TransformComponent>(m_player)) return;
                    auto& myTF     = ctx.registry->getComponent<engine::TransformComponent>(ctx.entity);
                    auto& playerTF = ctx.registry->getComponent<engine::TransformComponent>(m_player);
                    auto& vel      = ctx.registry->getComponent<engine::VelocityComponent>(ctx.entity);

                    float dx = playerTF.position.x - myTF.position.x;
                    constexpr float CHASE_SPEED = 140.f;
                    vel.linear.x = (dx > 0.f ? 1.f : -1.f) * CHASE_SPEED;
                    ctx.blackboard->at("distToPlayer") = std::abs(dx);
                },
                nullptr
            });

            // RETURN state — walk back to spawn point.
            sm.addState(engine::State{
                "return",
                nullptr,
                [](engine::StateContext& ctx) {
                    auto& tf  = ctx.registry->getComponent<engine::TransformComponent>(ctx.entity);
                    auto& vel = ctx.registry->getComponent<engine::VelocityComponent>(ctx.entity);
                    float spawnX = ctx.blackboard->at("spawnX");
                    float dx = spawnX - tf.position.x;
                    vel.linear.x = (std::abs(dx) > 4.f) ? (dx > 0.f ? 80.f : -80.f) : 0.f;
                    ctx.blackboard->at("distToSpawn") = std::abs(dx);
                },
                nullptr
            });

            sm.set("distToPlayer", 9999.f);
            sm.set("distToSpawn",  0.f);

            // ── Transitions ──────────────────────────────────────────────────
            // patrol → chase: if player is within 200px.
            sm.addTransition("patrol", engine::Transition{
                "chase",
                [this](engine::StateContext& ctx) -> bool {
                    if (!ctx.registry->hasComponent<engine::TransformComponent>(m_player)) return false;
                    auto& myTF     = ctx.registry->getComponent<engine::TransformComponent>(ctx.entity);
                    auto& playerTF = ctx.registry->getComponent<engine::TransformComponent>(m_player);
                    float dx = std::abs(playerTF.position.x - myTF.position.x);
                    ctx.blackboard->at("distToPlayer") = dx;
                    return dx < 200.f;
                }
            });

            // chase → return: if player goes farther than 400px.
            sm.addTransition("chase", engine::Transition{
                "return",
                [this](engine::StateContext& ctx) -> bool {
                    if (!ctx.registry->hasComponent<engine::TransformComponent>(m_player)) return false;
                    auto& myTF     = ctx.registry->getComponent<engine::TransformComponent>(ctx.entity);
                    auto& playerTF = ctx.registry->getComponent<engine::TransformComponent>(m_player);
                    return std::abs(playerTF.position.x - myTF.position.x) > 400.f;
                }
            });

            // return → patrol: when close enough to spawn.
            sm.addTransition("return", engine::Transition{
                "patrol",
                [](engine::StateContext& ctx) -> bool {
                    return ctx.blackboard->at("distToSpawn") < 10.f;
                }
            });

            sm.setInitialState("patrol");
            m_registry.addComponent<engine::StateMachineComponent>(e, std::move(sm));
            m_enemies.push_back(e);
        };

        spawnEnemy(400.f, 300.f, 500.f);
        spawnEnemy(750.f, 650.f, 900.f);
        spawnEnemy(1100.f, 1000.f, 1250.f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Player input handling
    // ─────────────────────────────────────────────────────────────────────────
    void handlePlayerInput(float dt, engine::InputManager& input) {
        auto& vel  = m_registry.getComponent<engine::VelocityComponent>(m_player);
        auto& anim = m_registry.getComponent<engine::AnimatorComponent>(m_player);
        auto& spr  = m_registry.getComponent<engine::SpriteComponent>(m_player);
        auto& em   = m_registry.getComponent<engine::EmitterComponent>(m_player);

        constexpr float SPEED = 180.f;
        constexpr float JUMP  = -390.f;

        float moveX = 0.f;
        if (input.isHeld(sf::Keyboard::A) || input.isHeld(sf::Keyboard::Left))  moveX -= 1.f;
        if (input.isHeld(sf::Keyboard::D) || input.isHeld(sf::Keyboard::Right)) moveX += 1.f;
        vel.linear.x = moveX * SPEED;

        if ((input.isPressed(sf::Keyboard::Space) || input.isPressed(sf::Keyboard::W))
            && m_onGround)
        {
            vel.linear.y   = JUMP;
            m_onGround     = false;
            m_camera.addTrauma(0.12f);
            // Burst dust particles on jump.
            engine::EmitterComponent jumpBurst;
            jumpBurst.isBurst      = true;
            jumpBurst.burstCount   = 12;
            jumpBurst.speed        = 60.f;
            jumpBurst.speedSpread  = 30.f;
            jumpBurst.direction    = 90.f;
            jumpBurst.directionSpread = 60.f;
            jumpBurst.lifetime     = 0.35f;
            jumpBurst.sizeStart    = 7.f;
            jumpBurst.sizeEnd      = 0.f;
            jumpBurst.colorStart   = sf::Color(230, 220, 200, 200);
            jumpBurst.colorEnd     = sf::Color(230, 220, 200, 0);
            jumpBurst.gravityScale = 0.f;

            auto& tf = m_registry.getComponent<engine::TransformComponent>(m_player);
            m_particleSystem->burst(tf.position + sf::Vector2f{0.f, 22.f}, jumpBurst, 12);
        }

        // Running dust.
        em.emissionRate = (m_onGround && std::abs(moveX) > 0.1f) ? 40.f : 0.f;

        // Sprite direction.
        if (moveX < 0.f) spr.sprite.setScale(-1.f, 1.f);
        else if (moveX > 0.f) spr.sprite.setScale(1.f, 1.f);

        // Animation state.
        if (!m_onGround)                    anim.play("jump");
        else if (std::abs(moveX) > 0.01f)   anim.play("walk");
        else                                anim.play("idle");

        // Dynamic zoom.
        float spd = std::sqrt(vel.linear.x*vel.linear.x + vel.linear.y*vel.linear.y);
        m_camera.setZoom(1.f - std::min(spd / 1200.f, 0.3f));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Debug / utility keys
    // ─────────────────────────────────────────────────────────────────────────
    void handleDebugKeys(engine::InputManager& input) {
        if (input.isPressed(sf::Keyboard::F1)) m_debugRenderer->toggle();

        if (input.isPressed(sf::Keyboard::F5)) {
            engine::SceneSerializer::saveToFile(m_registry, "quicksave.json");
        }
        if (input.isPressed(sf::Keyboard::F9)) {
            engine::SceneSerializer::loadFromFile(m_registry, "quicksave.json");
        }

        // Camera shake stress test.
        if (input.isPressed(sf::Keyboard::H)) m_camera.addTrauma(0.8f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Collision callback
    // ─────────────────────────────────────────────────────────────────────────
    void onCollision(const engine::CollisionEvent& evt) {
        bool involvesPlayer = (evt.entityA == m_player || evt.entityB == m_player);

        // Ground detection.
        if (involvesPlayer && evt.penetration.y < 0.f) {
            m_onGround = true;
        }

        // Player takes damage from enemies.
        if (involvesPlayer) {
            engine::Entity other = (evt.entityA == m_player) ? evt.entityB : evt.entityA;
            bool isEnemy = m_registry.hasComponent<engine::TagComponent>(other) &&
                           m_registry.getComponent<engine::TagComponent>(other).tag == "Enemy";

            if (isEnemy && m_registry.hasComponent<engine::HealthComponent>(m_player)) {
                bool hit = m_registry.getComponent<engine::HealthComponent>(m_player)
                               .applyDamage(10.f, 0.8f);
                if (hit) {
                    m_camera.addTrauma(0.4f);
                    // m_audio.playSound("hurt", 0.1f);

                    // Spawn hit particles at player position.
                    auto& tf = m_registry.getComponent<engine::TransformComponent>(m_player);
                    engine::EmitterComponent hitBurst;
                    hitBurst.isBurst = true; hitBurst.burstCount = 16;
                    hitBurst.speed = 120.f; hitBurst.speedSpread = 60.f;
                    hitBurst.direction = 270.f; hitBurst.directionSpread = 180.f;
                    hitBurst.lifetime = 0.5f; hitBurst.sizeStart = 5.f; hitBurst.sizeEnd = 0.f;
                    hitBurst.colorStart = sf::Color(255, 60, 60, 255);
                    hitBurst.colorEnd   = sf::Color(255,  0,  0,   0);
                    m_particleSystem->burst(tf.position, hitBurst, 16);
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Prune dead enemies (spawn death burst then destroy)
    // ─────────────────────────────────────────────────────────────────────────
    void pruneDeadEntities() {
        for (auto it = m_enemies.begin(); it != m_enemies.end(); ) {
            engine::Entity e = *it;
            if (m_registry.hasComponent<engine::HealthComponent>(e) &&
                m_registry.getComponent<engine::HealthComponent>(e).isDead)
            {
                auto& tf = m_registry.getComponent<engine::TransformComponent>(e);
                engine::EmitterComponent burst;
                burst.isBurst = true; burst.burstCount = 24;
                burst.speed = 150.f; burst.speedSpread = 80.f;
                burst.direction = 270.f; burst.directionSpread = 180.f;
                burst.lifetime = 0.7f; burst.sizeStart = 6.f; burst.sizeEnd = 0.f;
                burst.colorStart = sf::Color(255, 120, 30, 255);
                burst.colorEnd   = sf::Color(255,  50,  0,   0);
                m_particleSystem->burst(tf.position, burst, 24);
                m_registry.destroyEntity(e);
                it = m_enemies.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // HUD rendering (screen space — after view reset)
    // ─────────────────────────────────────────────────────────────────────────
    void renderHUD(sf::RenderWindow& window) {
        if (!m_registry.hasComponent<engine::HealthComponent>(m_player)) return;
        float hp = m_registry.getComponent<engine::HealthComponent>(m_player).normalised();

        // Health bar background.
        sf::RectangleShape bg({ 200.f, 16.f });
        bg.setFillColor(sf::Color(60, 20, 20, 200));
        bg.setPosition(16.f, 16.f);
        window.draw(bg);

        // Health bar fill — interpolates red→green based on HP.
        sf::RectangleShape fill({ 200.f * hp, 16.f });
        sf::Color barColor(
            static_cast<sf::Uint8>((1.f - hp) * 255),
            static_cast<sf::Uint8>(hp * 200),
            40, 220);
        fill.setFillColor(barColor);
        fill.setPosition(16.f, 16.f);
        window.draw(fill);

        // Particle count debug (useful for tuning pool size).
        // (Rendered as a small bar — no font needed.)
    }

    // ── Member data ───────────────────────────────────────────────────────────
    sf::RenderWindow&  m_window;
    engine::Camera     m_camera;
    engine::EventBus   m_eventBus;
    engine::AssetManager m_assets;
    engine::AudioManager m_audio;

    engine::Entity m_player         { engine::INVALID_ENTITY };
    engine::Entity m_dustEmitterEntity { engine::INVALID_ENTITY };
    bool           m_onGround       { false };
    std::vector<engine::Entity> m_enemies;

    std::unique_ptr<engine::RenderSystem>       m_renderSystem;
    std::unique_ptr<engine::PhysicsSystem>      m_physicsSystem;
    std::unique_ptr<engine::AnimationSystem>    m_animationSystem;
    std::unique_ptr<engine::DebugRenderer>      m_debugRenderer;
    std::unique_ptr<engine::ParticleSystem>     m_particleSystem;
    std::unique_ptr<engine::TileMapSystem>      m_tileMapSystem;
    std::unique_ptr<engine::StateMachineSystem> m_stateMachineSystem;

    engine::SubscriptionHandle m_collisionHandle;

    sf::Texture m_playerTex;
    std::unordered_map<std::string, std::unique_ptr<sf::Texture>> m_fallbackTextures;
};
