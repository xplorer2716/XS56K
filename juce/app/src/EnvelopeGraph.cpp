/*
XS56K - a realtime editor for the AKAI S5000/S6000 samplers
Copyright (C) 2026 https://github.com/xplorer2716

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "EnvelopeGraph.hpp"

#include <algorithm>
#include <cmath>

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-095, RQ-APP-091, ADR-APP-004 (DEC-APP-031), TASK-APP-044]
    namespace
    {
        constexpr float FULL_LEVEL = 100.0f;
        constexpr float GRAB_DISTANCE = 2.0f;  // a breakpoint is grabbed within twice its size
    }

    EnvelopeGraph::EnvelopeGraph(std::span<const layouts::EnvelopeStage> stages,
                                 const std::function<std::optional<std::size_t>(std::string_view)>& indexOf, juce::Colour colour)
        : _colour(colour)
    {
        for (const layouts::EnvelopeStage& stage : stages)
        {
            Stage shown;
            shown.sustain = stage.sustain;
            if (!stage.sustain)
            {
                shown.time = indexOf(stage.time);
                if (!stage.level.empty())
                    shown.level = indexOf(stage.level);
                shown.fixedLevel = static_cast<float>(stage.fixedLevel) / FULL_LEVEL;
            }
            _stages.push_back(shown);
        }
        setRepaintsOnMouseActivity(false);
    }

    void EnvelopeGraph::show(const std::vector<EditValue>& values)
    {
        _values = values;
        repaint();
    }

    float EnvelopeGraph::fractionOf(std::size_t index) const
    {
        if (index >= _values.size())
            return 0.0f;
        const EditValue& value = _values[index];
        if (value.max <= value.min)
            return 0.0f;
        return std::clamp(static_cast<float>(value.value - value.min) / static_cast<float>(value.max - value.min), 0.0f, 1.0f);
    }

    std::int64_t EnvelopeGraph::valueAt(std::size_t index, float fraction) const
    {
        const EditValue& value = _values[index];
        return value.min + static_cast<std::int64_t>(std::lround(std::clamp(fraction, 0.0f, 1.0f) * static_cast<float>(value.max - value.min)));
    }

    EnvelopeGraph::Shape EnvelopeGraph::shapeOf(std::size_t stage) const
    {
        if (_dragged == stage)
            return _preview;
        const Stage& shown = _stages[stage];
        Shape shape;
        shape.time = shown.time.has_value() ? fractionOf(*shown.time) : 0.0f;
        shape.level = shown.level.has_value() ? fractionOf(*shown.level) : shown.fixedLevel;
        return shape;
    }

    juce::Rectangle<float> EnvelopeGraph::plot() const
    {
        // The breakpoints stay whole inside the graph.
        return getLocalBounds().toFloat().reduced(tokens::kEnvelopeHandleSize);
    }

    std::vector<juce::Point<float>> EnvelopeGraph::points(std::vector<std::optional<std::size_t>>* handles) const
    {
        const auto area = plot();
        const float share = area.getWidth() / static_cast<float>(std::max<std::size_t>(1, _stages.size()));
        std::vector<juce::Point<float>> shown{{area.getX(), area.getBottom()}};
        if (handles != nullptr)
            handles->emplace_back(std::nullopt);
        float level = 0.0f;
        for (std::size_t stage = 0; stage < _stages.size(); ++stage)
        {
            const Shape shape = shapeOf(stage);
            if (_stages[stage].sustain)
            {
                shown.emplace_back(shown.back().x + share, area.getBottom() - area.getHeight() * level);
                if (handles != nullptr)
                    handles->emplace_back(std::nullopt);
                continue;
            }
            level = shape.level;
            shown.emplace_back(shown.back().x + share * shape.time, area.getBottom() - area.getHeight() * level);
            if (handles != nullptr)
                handles->emplace_back(stage);
        }
        return shown;
    }

    void EnvelopeGraph::paint(juce::Graphics& g)
    {
        using namespace tokens;
        g.setColour(colourOf(kEnvelopeSurface));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), kRadiusControl);
        // Pending while one of its values is being written, as the knobs are. [RQ-APP-052]
        const auto pendingAt = [this](const std::optional<std::size_t>& index) { return index && *index < _values.size() && _values[*index].pending; };
        const bool pending =
            std::ranges::any_of(_stages, [&pendingAt](const Stage& stage) { return pendingAt(stage.time) || pendingAt(stage.level); });
        const juce::Colour colour = pending ? colourOf(kColourStatusPending) : _colour;
        std::vector<std::optional<std::size_t>> handles;
        const auto shown = points(&handles);
        juce::Path line;
        line.startNewSubPath(shown.front());
        for (std::size_t i = 1; i < shown.size(); ++i)
            line.lineTo(shown[i]);
        juce::Path area = line;
        area.lineTo(shown.back().x, plot().getBottom());
        area.closeSubPath();
        g.setColour(colour.withAlpha(kOpacityFill));
        g.fillPath(area);
        g.setColour(colour);
        g.strokePath(line, juce::PathStrokeType(kStrokeArc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colourOf(kEnvelopeHandle));
        for (std::size_t i = 0; i < shown.size(); ++i)
        {
            if (handles[i].has_value())
                g.fillEllipse(juce::Rectangle<float>(kEnvelopeHandleSize, kEnvelopeHandleSize).withCentre(shown[i]));
        }
    }

    void EnvelopeGraph::mouseDown(const juce::MouseEvent& event)
    {
        std::vector<std::optional<std::size_t>> handles;
        const auto shown = points(&handles);
        float nearest = tokens::kEnvelopeHandleSize * GRAB_DISTANCE;
        std::optional<std::size_t> grabbed;
        for (std::size_t i = 0; i < shown.size(); ++i)
        {
            const float distance = shown[i].getDistanceFrom(event.position);
            if (handles[i].has_value() && distance <= nearest)
            {
                nearest = distance;
                grabbed = handles[i];
            }
        }
        if (grabbed.has_value())
            _preview = shapeOf(*grabbed);  // the shape held, before the drag moves it
        _dragged = grabbed;
    }

    void EnvelopeGraph::mouseDrag(const juce::MouseEvent& event)
    {
        if (!_dragged.has_value())
            return;
        const std::size_t stage = *_dragged;
        const auto area = plot();
        const float share = area.getWidth() / static_cast<float>(std::max<std::size_t>(1, _stages.size()));
        // Where the stage starts: the end of the stage before it, as drawn.
        const std::optional<std::size_t> held = _dragged;
        _dragged.reset();
        std::vector<std::optional<std::size_t>> handles;
        const auto shown = points(&handles);
        _dragged = held;
        const auto at = std::ranges::find(handles, std::optional<std::size_t>(stage));
        const float start = shown[static_cast<std::size_t>(at - handles.begin()) - 1].x;
        const Stage& dragged = _stages[stage];
        _preview.time = std::clamp((event.position.x - start) / share, 0.0f, 1.0f);
        if (dragged.level.has_value())
            _preview.level = std::clamp((area.getBottom() - event.position.y) / area.getHeight(), 0.0f, 1.0f);
        repaint();
        if (!onValue)
            return;
        if (dragged.time.has_value() && *dragged.time < _values.size())
        {
            const std::int64_t time = valueAt(*dragged.time, _preview.time);
            if (time != _values[*dragged.time].value)
                onValue(*dragged.time, time);
        }
        if (dragged.level.has_value() && *dragged.level < _values.size())
        {
            const std::int64_t level = valueAt(*dragged.level, _preview.level);
            if (level != _values[*dragged.level].value)
                onValue(*dragged.level, level);
        }
    }

    void EnvelopeGraph::mouseUp(const juce::MouseEvent&)
    {
        _dragged.reset();  // the graph shows what the model holds again [RQ-APP-091]
        repaint();
    }
}
