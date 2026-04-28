#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// StateMachine.hpp  —  Hierarchical finite state machine as an ECS component
//
// WHY A STATE MACHINE FOR AI?
//   Enemies, NPCs, and game objects have distinct behaviours:
//   • A guard: Patrol → Alert → Chase → Attack → Return
//   • A door: Closed → Opening → Open → Closing
//   • A player: Idle → Run → Jump → Fall → Hurt
//
//   A state machine makes these transitions explicit and debuggable.
//   Without one, behaviour lives in a tangle of if/else chains.
//
// THIS IMPLEMENTATION:
//   State       — name + three callbacks: onEnter, onUpdate, onExit.
//   Transition  — a condition (predicate) + target state name.
//   StateMachine— holds states and evaluates transitions every update.
//
// DESIGN CHOICE: callbacks via std::function
//   Each state's logic is a std::function — you can pass a lambda, a member
//   function wrapper, or a free function. This avoids a class-per-state
//   inheritance hierarchy for simple machines.
//
// BLACKBOARD:
//   A std::unordered_map<string, float> "blackboard" lets states share data
//   (e.g., "distanceToPlayer", "health") without coupling state logic to
//   specific component types. Classic AI design pattern.
// ─────────────────────────────────────────────────────────────────────────────

#include "ECS.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace engine {

// Context passed to every state callback.
struct StateContext {
    Entity                              entity;
    Registry*                           registry  { nullptr };
    float                               dt        { 0.f };
    // Blackboard: shared key-value store for this state machine.
    // Using float covers most AI variables (distances, timers, HP).
    std::unordered_map<std::string, float>* blackboard { nullptr };
};

// ─────────────────────────────────────────────────────────────────────────────
// State  —  A single node in the state machine
// ─────────────────────────────────────────────────────────────────────────────
struct State {
    std::string name;

    // Lifecycle callbacks — any of these can be nullptr (checked before call).
    std::function<void(StateContext&)> onEnter;
    std::function<void(StateContext&)> onUpdate;
    std::function<void(StateContext&)> onExit;

    State() = default;
    State(std::string n,
          std::function<void(StateContext&)> enter  = nullptr,
          std::function<void(StateContext&)> update = nullptr,
          std::function<void(StateContext&)> exit   = nullptr)
        : name(std::move(n))
        , onEnter(std::move(enter))
        , onUpdate(std::move(update))
        , onExit(std::move(exit))
    {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Transition  —  A conditional edge from one state to another
// ─────────────────────────────────────────────────────────────────────────────
struct Transition {
    std::string                       targetState;
    // condition returns true when the transition should fire.
    std::function<bool(StateContext&)> condition;

    Transition(std::string target, std::function<bool(StateContext&)> cond)
        : targetState(std::move(target)), condition(std::move(cond)) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// StateMachineComponent  —  ECS component that embeds a full state machine
// ─────────────────────────────────────────────────────────────────────────────
struct StateMachineComponent {
    // States and transitions are registered at scene load, then driven by update().
    std::unordered_map<std::string, State>               states;
    std::unordered_map<std::string, std::vector<Transition>> transitions;

    std::string currentState;
    bool        started { false };

    // The blackboard: shared mutable data for all states.
    std::unordered_map<std::string, float> blackboard;

    // ── Builder API ───────────────────────────────────────────────────────────

    StateMachineComponent& addState(State s) {
        std::string key = s.name;
        states.emplace(std::move(key), std::move(s));
        return *this;
    }

    // Fluent: machine.addState(...).addTransition(...).addTransition(...)
    StateMachineComponent& addTransition(const std::string& fromState, Transition t) {
        transitions[fromState].push_back(std::move(t));
        return *this;
    }

    StateMachineComponent& setInitialState(const std::string& name) {
        currentState = name;
        return *this;
    }

    // ── Blackboard helpers ────────────────────────────────────────────────────
    void  set(const std::string& key, float value) { blackboard[key] = value; }
    float get(const std::string& key, float def = 0.f) const {
        auto it = blackboard.find(key);
        return (it != blackboard.end()) ? it->second : def;
    }

    // ── Runtime: called by StateMachineSystem ─────────────────────────────────
    void update(Entity entity, Registry& registry, float dt) {
        if (currentState.empty()) return;

        auto stateIt = states.find(currentState);
        if (stateIt == states.end()) return;

        StateContext ctx { entity, &registry, dt, &blackboard };

        // Call onEnter exactly once when first entering a state.
        if (!started) {
            started = true;
            if (stateIt->second.onEnter) stateIt->second.onEnter(ctx);
        }

        // Run the current state's update logic.
        if (stateIt->second.onUpdate) stateIt->second.onUpdate(ctx);

        // Evaluate transitions — first matching condition wins.
        auto transIt = transitions.find(currentState);
        if (transIt != transitions.end()) {
            for (const auto& trans : transIt->second) {
                if (trans.condition && trans.condition(ctx)) {
                    transition(trans.targetState, ctx);
                    break; // Only one transition per frame.
                }
            }
        }
    }

private:
    void transition(const std::string& target, StateContext& ctx) {
        // Call onExit for the current state.
        auto exitIt = states.find(currentState);
        if (exitIt != states.end() && exitIt->second.onExit) {
            exitIt->second.onExit(ctx);
        }

        currentState = target;

        // Call onEnter for the new state.
        auto enterIt = states.find(currentState);
        if (enterIt != states.end() && enterIt->second.onEnter) {
            enterIt->second.onEnter(ctx);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// StateMachineSystem  —  Drives all StateMachineComponents each tick
// ─────────────────────────────────────────────────────────────────────────────
class StateMachineSystem {
public:
    explicit StateMachineSystem(Registry& registry)
        : m_registry(registry)
    {
        m_signature = registry.buildSignature<StateMachineComponent>();
    }

    void update(float dt) {
        for (Entity e : m_registry.view(m_signature)) {
            auto& sm = m_registry.getComponent<StateMachineComponent>(e);
            sm.update(e, m_registry, dt);
        }
    }

private:
    Registry& m_registry;
    Signature m_signature;
};

} // namespace engine
