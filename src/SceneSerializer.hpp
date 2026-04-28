#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SceneSerializer.hpp  —  Save and load scene entity/component data as JSON
//
// DEPENDENCY: nlohmann/json (single-header, MIT licence).
//   Add to your project: https://github.com/nlohmann/json/releases
//   Drop json.hpp into src/third_party/ and the include below will find it.
//
// WHAT IS SERIALISED:
//   • Entity IDs and their component data.
//   • TransformComponent, VelocityComponent, TagComponent.
//   • (Textures/audio are referenced by name and reloaded from disk — we
//     don't serialise binary GPU data.)
//
// HOW TO EXTEND:
//   Add a to_json / from_json overload for your custom component type,
//   then register it in the serialize/deserialize dispatch tables below.
//
// JSON FORMAT EXAMPLE:
//   {
//     "entities": [
//       {
//         "id": 0,
//         "components": {
//           "Transform": { "x": 120.5, "y": 300.0, "rotation": 0.0,
//                          "scaleX": 1.0, "scaleY": 1.0 },
//           "Tag":       { "tag": "Player" },
//           "Velocity":  { "vx": 0.0, "vy": 0.0, "angular": 0.0 }
//         }
//       }
//     ]
//   }
//
// WHY JSON (not binary)?
//   • Human-readable — you can edit levels in a text editor or export from Tiled.
//   • Easy to version-control (diffs are meaningful).
//   • Slower and larger than binary — acceptable for levels, not per-frame state.
// ─────────────────────────────────────────────────────────────────────────────

// If you haven't added nlohmann/json yet, the include below will fail with a
// clear error. Get json.hpp from: https://github.com/nlohmann/json/releases/latest
#ifdef ENGINE_HAS_JSON
#include "third_party/json.hpp"
#endif

#include "ECS.hpp"
#include "Components.hpp"
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace engine {

#ifdef ENGINE_HAS_JSON
using json = nlohmann::json;

// ── to_json / from_json free functions ────────────────────────────────────────
// nlohmann/json finds these via ADL (argument-dependent lookup).
// This is the library's idiomatic extension point — no inheritance needed.

inline void to_json(json& j, const sf::Vector2f& v) {
    j = json{ {"x", v.x}, {"y", v.y} };
}
inline void from_json(const json& j, sf::Vector2f& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
}

inline void to_json(json& j, const TransformComponent& t) {
    j = json{
        {"position", t.position},
        {"rotation", t.rotation},
        {"scale",    t.scale   }
    };
}
inline void from_json(const json& j, TransformComponent& t) {
    j.at("position").get_to(t.position);
    j.at("rotation").get_to(t.rotation);
    j.at("scale"   ).get_to(t.scale   );
}

inline void to_json(json& j, const VelocityComponent& v) {
    j = json{ {"linear", v.linear}, {"angular", v.angular} };
}
inline void from_json(const json& j, VelocityComponent& v) {
    j.at("linear" ).get_to(v.linear );
    j.at("angular").get_to(v.angular);
}

inline void to_json(json& j, const TagComponent& t) {
    j = json{ {"tag", t.tag} };
}
inline void from_json(const json& j, TagComponent& t) {
    j.at("tag").get_to(t.tag);
}
#endif // ENGINE_HAS_JSON

// ─────────────────────────────────────────────────────────────────────────────
// SceneSerializer
// ─────────────────────────────────────────────────────────────────────────────
class SceneSerializer {
public:

#ifdef ENGINE_HAS_JSON

    // ── Save ──────────────────────────────────────────────────────────────────
    static void saveToFile(Registry& registry, const std::string& filepath) {
        json root;
        json& entitiesJson = root["entities"];

        // Iterate every potentially-live entity.
        // In a real engine, Registry::view(empty_signature) returns all live entities.
        // We approximate that here with a Tag-based approach for simplicity.
        auto sig = registry.buildSignature<TagComponent>();

        for (Entity e : registry.view(sig)) {
            json entityJson;
            entityJson["id"] = e;
            json& comps = entityJson["components"];

            // Serialise each component type if the entity has it.
            if (registry.hasComponent<TransformComponent>(e))
                comps["Transform"] = registry.getComponent<TransformComponent>(e);

            if (registry.hasComponent<VelocityComponent>(e))
                comps["Velocity"] = registry.getComponent<VelocityComponent>(e);

            if (registry.hasComponent<TagComponent>(e))
                comps["Tag"] = registry.getComponent<TagComponent>(e);

            entitiesJson.push_back(std::move(entityJson));
        }

        // Write to file with 2-space indentation for readability.
        std::ofstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("SceneSerializer: cannot open '" + filepath + "' for writing.");
        }
        file << root.dump(2);
    }

    // ── Load ──────────────────────────────────────────────────────────────────
    // Returns the list of newly-created entity IDs.
    static std::vector<Entity> loadFromFile(Registry& registry, const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("SceneSerializer: cannot open '" + filepath + "'.");
        }

        json root;
        file >> root;

        std::vector<Entity> created;

        for (const auto& entityJson : root.at("entities")) {
            Entity e = registry.createEntity();
            created.push_back(e);

            const auto& comps = entityJson.at("components");

            if (comps.contains("Transform")) {
                registry.addComponent<TransformComponent>(
                    e, comps["Transform"].get<TransformComponent>());
            }
            if (comps.contains("Velocity")) {
                registry.addComponent<VelocityComponent>(
                    e, comps["Velocity"].get<VelocityComponent>());
            }
            if (comps.contains("Tag")) {
                registry.addComponent<TagComponent>(
                    e, comps["Tag"].get<TagComponent>());
            }
            // Add more component types here as you extend the engine.
        }

        return created;
    }

    // ── Convenience: save to string (useful for network sync or quicksave) ────
    static std::string saveToString(Registry& registry) {
        // Temporarily save to a stringstream by adapting the file approach.
        // In production you'd refactor saveToFile to accept a generic ostream&.
        const std::string tmp = ".__engine_tmp_save.json";
        saveToFile(registry, tmp);
        std::ifstream f(tmp);
        std::string s((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
        return s;
    }

#else // ENGINE_HAS_JSON not defined

    // Stub implementations when nlohmann/json is not available.
    // They compile fine but do nothing, so the rest of the engine still builds.
    static void saveToFile(Registry&, const std::string&) {
        // Define ENGINE_HAS_JSON and add nlohmann/json to use serialisation.
    }
    static std::vector<Entity> loadFromFile(Registry&, const std::string&) {
        return {};
    }
    static std::string saveToString(Registry&) { return "{}"; }

#endif
};

} // namespace engine
