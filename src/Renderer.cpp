#include "Renderer.hpp"

#include <algorithm>
#include <utility>

void Renderer::BeginFrame()
{
    m_queue.clear();
    m_sequence = 0;
}

void Renderer::Submit(float depth, DrawCommand command)
{
    if (!command) {
        return;
    }

    m_queue.push_back(Entry{ depth, m_sequence++, std::move(command) });
}

void Renderer::Submit(Vector2 screenPosition, DrawCommand command)
{
    Submit(screenPosition.y, std::move(command));
}

void Renderer::Flush()
{
    // Back-to-front: the smallest isometric Y is the furthest away, so it is
    // painted first. stable_sort keeps the submission order for equal depths,
    // which is what makes "player standing on a tile" resolve deterministically.
    std::stable_sort(m_queue.begin(), m_queue.end(),
                     [](const Entry& lhs, const Entry& rhs) {
                         return lhs.depth < rhs.depth;
                     });

    for (const Entry& entry : m_queue) {
        if (entry.command) {
            entry.command();
        }
    }

    m_lastDrawnCount = m_queue.size();
    m_queue.clear();
}

std::size_t Renderer::PendingCount() const noexcept
{
    return m_queue.size();
}

std::size_t Renderer::LastDrawnCount() const noexcept
{
    return m_lastDrawnCount;
}
