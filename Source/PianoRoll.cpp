#include "PianoRoll.h"

PianoRoll::PianoRoll()
{
    // Inicialización del componente del piano roll
}

PianoRoll::~PianoRoll()
{
}

void PianoRoll::paint (juce::Graphics& g)
{
    // Fondo oscuro típico de DAW
    g.fillAll (juce::Colour::fromRGB(30, 30, 30));

    // Dibujar líneas de la cuadrícula del piano roll
    g.setColour (juce::Colour::fromRGB(50, 50, 50));
    
    float heightPerPage = (float)getHeight() / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        g.drawHorizontalLine (static_int_cast<int>(i * heightPerPage), 0.0f, (float)getWidth());
    }

    // Texto informativo
    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText ("La Pauta Tune - Piano Roll View", getLocalBounds(), juce::Justification::centredTop, true);

    // Ejemplo seguro de conversión de Point<int> a Point<float> usando .toFloat()
    juce::Point<int> mouseIntPos (50, 50);
    juce::Point<float> mouseFloatPos = mouseIntPos.toFloat();
    
    g.setColour (juce::Colours::cyan);
    g.fillEllipse (mouseFloatPos.x, mouseFloatPos.y, 8.0f, 8.0f);
}

void PianoRoll::resized()
{
    // Ajustar elementos internos según el tamaño del componente
}

void PianoRoll::mouseDown (const juce::MouseEvent& event)
{
    // Convertir la posición del evento de entero a float de manera segura
    juce::Point<float> clickPos = event.getPosition().toFloat();
    
    // Lógica al hacer clic en las notas del piano roll
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& event)
{
    // Convertir la posición de arrastre a float
    juce::Point<float> dragPos = event.getPosition().toFloat();
    
    // Lógica al arrastrar notas
    repaint();
}
