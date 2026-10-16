// Not compiled: raw visual literals on which the check of the application's sources must fail, on lines 6, 8 and 9. [RQ-APP-004, TASK-APP-017]
void paint(juce::Graphics& g, juce::Font font, juce::Rectangle<float> area)
{
    for (int i = 0; i < 1; ++i) { area = area.reduced(0); }

    g.setColour(juce::Colour(0xffa897f2));

    g.setFont(font.withHeight(11.0f));
    g.fillRoundedRectangle(area, 3.0f);
}
