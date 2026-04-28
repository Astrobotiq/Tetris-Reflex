#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// TileMap.hpp  —  Grid-based world map with chunk-aware rendering
//
// WHAT IS A TILEMAP?
//   A 2D grid where each cell holds an integer "tile ID". A single texture atlas
//   (the "tileset") maps IDs to sub-rectangles on that texture. This is how
//   virtually every 2D game represents its world — efficient, compact, and fast.
//
// ARCHITECTURE:
//   TileMapData   — pure data: grid dimensions, tile IDs, tileset info.
//   TileLayer     — one z-ordered layer (background, foreground, collision…).
//   TileMap       — owns multiple layers; generates sf::VertexArray geometry.
//   TileMapSystem — queries the Registry for TileMap components and draws them.
//
// WHY sf::VertexArray?
//   Drawing one sf::Sprite per tile = thousands of draw calls per frame.
//   A VertexArray batches ALL tiles into a SINGLE draw call, regardless of
//   how many tiles are on screen. This is a core rendering optimisation.
//   We rebuild the vertex array only when the map changes (dirty flag).
//
// COORDINATE CONVENTIONS:
//   • Tile (col, row) → world position (col * tileW, row * tileH).
//   • Tile ID 0 = empty (not drawn).
//   • Tile IDs are 1-indexed: ID 1 = first tile in the atlas.
// ─────────────────────────────────────────────────────────────────────────────

#include "ECS.hpp"
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {

// ─────────────────────────────────────────────────────────────────────────────
// TileLayer  —  A single named layer of tiles
// ─────────────────────────────────────────────────────────────────────────────
struct TileLayer {
    std::string            name;
    std::vector<int>       tiles;        // Row-major: tiles[row * cols + col]
    int                    cols { 0 };
    int                    rows { 0 };
    float                  opacity { 1.f };
    bool                   visible { true };

    // Collision layer: if true, solid tiles generate ColliderComponents.
    bool                   isCollision { false };

    TileLayer() = default;
    TileLayer(std::string n, int cols_, int rows_)
        : name(std::move(n)), cols(cols_), rows(rows_)
    {
        // Value-initialise all tiles to 0 (empty). The parentheses form
        // of resize() value-initialises, while resize(n) for int is also 0.
        tiles.assign(static_cast<std::size_t>(cols_ * rows_), 0);
    }

    int  get(int col, int row) const { return tiles[static_cast<std::size_t>(row * cols + col)]; }
    void set(int col, int row, int id) { tiles[static_cast<std::size_t>(row * cols + col)] = id; }

    bool inBounds(int col, int row) const {
        return col >= 0 && col < cols && row >= 0 && row < rows;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TileSetInfo  —  Describes the texture atlas used by this map
// ─────────────────────────────────────────────────────────────────────────────
struct TileSetInfo {
    int tileW      { 16 };  // Pixels per tile, width
    int tileH      { 16 };  // Pixels per tile, height
    int columns    { 16 };  // How many tiles wide the atlas image is
    int firstGID   { 1  };  // First valid tile ID (GID = global tile ID)
    std::string texturePath;

    // Compute the sub-rect for a given tile ID.
    sf::IntRect rectForID(int id) const {
        int localID = id - firstGID;         // Convert to 0-based index
        int col = localID % columns;
        int row = localID / columns;
        return { col * tileW, row * tileH, tileW, tileH };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TileMapComponent  —  ECS component holding the full map
// ─────────────────────────────────────────────────────────────────────────────
struct TileMapComponent {
    std::vector<TileLayer> layers;
    TileSetInfo            tileSet;

    // Cached geometry: rebuilt when dirty = true.
    // mutable allows rebuilding inside a const render call.
    mutable sf::VertexArray geometry;
    mutable bool            dirty { true };

    // World offset (position of tile (0,0) in world space).
    sf::Vector2f            origin { 0.f, 0.f };

    TileMapComponent() = default;

    TileLayer& addLayer(const std::string& name, int cols, int rows) {
        layers.emplace_back(name, cols, rows);
        dirty = true;
        return layers.back();
    }

    TileLayer* getLayer(const std::string& name) {
        for (auto& layer : layers) {
            if (layer.name == name) return &layer;
        }
        return nullptr;
    }

    // Convert world position to tile coordinate (for collision queries).
    sf::Vector2i worldToTile(sf::Vector2f worldPos) const {
        int col = static_cast<int>((worldPos.x - origin.x) / tileSet.tileW);
        int row = static_cast<int>((worldPos.y - origin.y) / tileSet.tileH);
        return { col, row };
    }

    // Check if a world-space rect overlaps any solid tile in collision layers.
    bool isSolid(sf::FloatRect worldRect) const {
        for (const auto& layer : layers) {
            if (!layer.isCollision) continue;
            int c0 = static_cast<int>((worldRect.left - origin.x) / tileSet.tileW);
            int r0 = static_cast<int>((worldRect.top  - origin.y) / tileSet.tileH);
            int c1 = static_cast<int>((worldRect.left + worldRect.width  - 1 - origin.x) / tileSet.tileW);
            int r1 = static_cast<int>((worldRect.top  + worldRect.height - 1 - origin.y) / tileSet.tileH);
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    if (layer.inBounds(c, r) && layer.get(c, r) != 0)
                        return true;
                }
            }
        }
        return false;
    }

    // Rebuild the VertexArray from all visible layers.
    // Called lazily by TileMapSystem when dirty == true.
    void rebuildGeometry(const sf::Texture& tex) const {
        geometry.setPrimitiveType(sf::Quads);
        geometry.clear();

        for (const auto& layer : layers) {
            if (!layer.visible) continue;
            for (int row = 0; row < layer.rows; ++row) {
                for (int col = 0; col < layer.cols; ++col) {
                    int id = layer.get(col, row);
                    if (id == 0) continue; // Empty tile — skip.

                    sf::IntRect tr = tileSet.rectForID(id);
                    float x = origin.x + col * static_cast<float>(tileSet.tileW);
                    float y = origin.y + row * static_cast<float>(tileSet.tileH);
                    float w = static_cast<float>(tileSet.tileW);
                    float h = static_cast<float>(tileSet.tileH);

                    // Each tile = 4 vertices (a quad).
                    // Position in world space, UV in texture space.
                    sf::Uint8 alpha = static_cast<sf::Uint8>(layer.opacity * 255.f);
                    sf::Color col_  { 255, 255, 255, alpha };

                    geometry.append({{x,   y  }, col_, {(float)tr.left,            (float)tr.top           }});
                    geometry.append({{x+w, y  }, col_, {(float)(tr.left + tr.width),(float)tr.top           }});
                    geometry.append({{x+w, y+h}, col_, {(float)(tr.left + tr.width),(float)(tr.top+tr.height)}});
                    geometry.append({{x,   y+h}, col_, {(float)tr.left,            (float)(tr.top+tr.height)}});
                }
            }
        }
        dirty = false;
        (void)tex; // Texture is bound at draw time by TileMapSystem.
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TileMapSystem  —  Renders all TileMapComponent entities
// ─────────────────────────────────────────────────────────────────────────────
class TileMapSystem {
public:
    explicit TileMapSystem(Registry& registry, AssetManager& assets)
        : m_registry(registry), m_assets(assets)
    {
        m_signature = registry.buildSignature<TileMapComponent>();
    }

    void render(sf::RenderWindow& window) {
        for (Entity e : m_registry.view(m_signature)) {
            auto& map = m_registry.getComponent<TileMapComponent>(e);
            if (map.dirty) {
                // Try to get the texture; skip rendering if not loaded yet.
                try {
                    const sf::Texture& tex = m_assets.getTexture("tileset");
                    map.rebuildGeometry(tex);
                } catch (...) { continue; }
            }

            // sf::RenderStates bundles the texture + transform for the draw call.
            // This is what makes the single draw call work — SFML binds the
            // texture once and draws all quads in one GPU submission.
            sf::RenderStates states;
            try {
                states.texture = &m_assets.getTexture("tileset");
            } catch (...) { continue; }

            window.draw(map.geometry, states);
        }
    }

private:
    Registry&     m_registry;
    AssetManager& m_assets;
    Signature     m_signature;
};

} // namespace engine

// ── Include guard for AssetManager dependency ─────────────────────────────────
#include "AssetManager.hpp"
