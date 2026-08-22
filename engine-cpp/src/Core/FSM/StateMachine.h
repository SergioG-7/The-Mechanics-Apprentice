#pragma once
#include <functional>
#include <unordered_map>

// Máquina de estados genérica: cualquier enum puede usarla registrando
// callbacks de Enter/Update/Exit por estado. Player y Enemy la usan cada uno con su propio enum.
template <typename StateEnum>
class StateMachine {
public:
    struct StateCallbacks {
        std::function<void()> onEnter;
        std::function<void(float)> onUpdate; // recibe dt
        std::function<void()> onExit;
    };

    void RegisterState(StateEnum state, StateCallbacks callbacks) {
        m_states[state] = std::move(callbacks);
    }

    void ChangeState(StateEnum newState) {
        if (m_hasState && newState == m_currentState) return;

        if (m_hasState) {
            auto it = m_states.find(m_currentState);
            if (it != m_states.end() && it->second.onExit) it->second.onExit();
        }

        m_currentState = newState;
        m_hasState = true;

        auto it = m_states.find(m_currentState);
        if (it != m_states.end() && it->second.onEnter) it->second.onEnter();
    }

    void Update(float dt) {
        if (!m_hasState) return;
        auto it = m_states.find(m_currentState);
        if (it != m_states.end() && it->second.onUpdate) it->second.onUpdate(dt);
    }

    StateEnum GetCurrentState() const { return m_currentState; }
    bool Is(StateEnum state) const { return m_hasState && m_currentState == state; }

private:
    std::unordered_map<StateEnum, StateCallbacks> m_states;
    StateEnum m_currentState{};
    bool m_hasState = false;
};
