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
#include "StatusBar.hpp"

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-010, RQ-APP-012, RQ-APP-013, RQ-APP-019, RQ-APP-005, ADR-APP-002 (DEC-APP-013), TASK-APP-017]
    namespace
    {
        constexpr int SPINNER_FRAMES = 24;
        constexpr float SPINNER_SWEEP = juce::MathConstants<float>::pi * 1.5f;
        constexpr const char* DISMISS_TEXT = "Dismiss";
        constexpr const char* FAILED_SEPARATOR = " failed: ";
        constexpr int CONNECTION_SHARE = 3;  // the connection text takes a third of the line
        constexpr int TOTAL_SHARES = 9;

        juce::String connectionText(const ConnectionStatus& connection)
        {
            switch (connection.state)
            {
                case ConnectionState::Disconnected:
                    return "Disconnected";
                case ConnectionState::Connecting:
                    return "Connecting" + ELLIPSIS;
                case ConnectionState::Connected:
                    if (connection.sampler)
                        return "Connected: " + juce::String(connection.sampler->model) + " \"" + juce::String(connection.sampler->name) + "\", OS " +
                               juce::String(connection.sampler->osVersion) + ", DeviceID " + juce::String(connection.sampler->deviceId);
                    return "Connected";
                case ConnectionState::Failed:
                    break;
            }
            return "Not connected: " + juce::String(connection.reason);
        }
    }

    StatusBar::StatusBar(LookAndFeel& lookAndFeel) : _lookAndFeel(lookAndFeel)
    {
        using namespace tokens;
        for (juce::Label* label : {&_connection, &_job, &_problem})
        {
            label->setFont(_lookAndFeel.font(kFontCaptionSize));
            label->setMinimumHorizontalScale(1.0f);
            label->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(*label);
        }
        _job.setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
        _problem.setColour(juce::Label::textColourId, colourOf(kColourStatusError));
        _dismiss.setButtonText(DISMISS_TEXT);
        _dismiss.onClick = [this] {
            if (onDismiss)
                onDismiss();
        };
        addChildComponent(_dismiss);
        show({}, std::nullopt, std::nullopt);
    }

    void StatusBar::show(const ConnectionStatus& connection, const std::optional<std::string>& busy, const std::optional<Problem>& problem)
    {
        _state = connection.state;
        const juce::String text = connectionText(connection);
        _connection.setText(text, juce::dontSendNotification);
        _job.setText(busy ? juce::String(*busy) : juce::String(), juce::dontSendNotification);
        if (busy && !_busy)
            startTimer(static_cast<int>(tokens::kMotionBusy.count() / SPINNER_FRAMES));
        else if (!busy && _busy)
            stopTimer();
        _busy = busy.has_value();

        const juce::String problemText = problem ? juce::String(problem->action) + FAILED_SEPARATOR + juce::String(problem->reason) : juce::String();
        _problem.setText(problemText, juce::dontSendNotification);
        _dismiss.setVisible(problem.has_value());
        repaint();
    }

    void StatusBar::timerCallback()
    {
        _angle += juce::MathConstants<float>::twoPi / SPINNER_FRAMES;
        repaint();
    }

    void StatusBar::paint(juce::Graphics& g)
    {
        using namespace tokens;
        g.fillAll(colourOf(kColourSurfacePanel));
        g.setColour(colourOf(kColourBorderDefault));
        g.fillRect(getLocalBounds().removeFromTop(pixels(kStrokeHairline)));

        // The connection dot: filled when connected or failed, an outline otherwise; the word beside it says the same. [RQ-APP-005]
        auto line = getLocalBounds().reduced(pixels(kSpaceInside), 0);
        const auto dotArea = line.removeFromLeft(pixels(kStatusDotSize)).toFloat().withSizeKeepingCentre(kStatusDotSize, kStatusDotSize);
        if (_state == ConnectionState::Connected || _state == ConnectionState::Failed)
        {
            g.setColour(colourOf(_state == ConnectionState::Connected ? kStatusDotConnected : kStatusDotFailed));
            g.fillEllipse(dotArea);
        }
        else
        {
            g.setColour(colourOf(kColourBorderDefault));
            g.drawEllipse(dotArea.reduced(kStrokeHairline), kStrokeHairline);
        }

        if (_busy)
        {
            const auto spinner = _job.getBounds().toFloat().withWidth(kStatusSpinnerSize).translated(-(kStatusSpinnerSize + kSpaceBetween), 0.0f)
                                     .withSizeKeepingCentre(kStatusSpinnerSize, kStatusSpinnerSize)
                                     .reduced(kStrokeFocus);
            juce::Path arc;
            arc.addCentredArc(spinner.getCentreX(), spinner.getCentreY(), spinner.getWidth() / 2, spinner.getHeight() / 2, _angle, 0.0f,
                              SPINNER_SWEEP, true);
            g.setColour(colourOf(kStatusSpinner));
            g.strokePath(arc, juce::PathStrokeType(kStrokeFocus));
        }
    }

    void StatusBar::resized()
    {
        using namespace tokens;
        auto line = getLocalBounds().reduced(pixels(kSpaceInside), 0);
        line.removeFromLeft(pixels(kStatusDotSize) + pixels(kSpaceBetween));
        const int share = line.getWidth() / TOTAL_SHARES;
        _connection.setBounds(line.removeFromLeft(share * CONNECTION_SHARE));
        line.removeFromLeft(pixels(kStatusSpinnerSize) + pixels(kSpaceBetween) * 2);
        _job.setBounds(line.removeFromLeft(share * 2));
        // The dismiss button keeps its place, shown or not, so that the problem's text does not move.
        _dismiss.setBounds(line.removeFromRight(_dismiss.getBestWidthForHeight(line.getHeight())).reduced(0, pixels(kSizeSpace1) / 2));
        line.removeFromRight(pixels(kSpaceBetween));
        _problem.setBounds(line);
    }
}
