# LA PAUTA TUNE V1 V1 — Foundation project

Proyecto JUCE para Windows, con formatos VST3 y Standalone.

## Estado real de esta entrega

Esta es una **base de proyecto compilable**, no una versión terminada de NewTone.
Incluye:
- CMake + JUCE 8.0.6.
- Objetivos VST3 y Standalone.
- Interfaz blanca con perillas grises.
- Parámetros guardables: Key, Scale, Center, Variation, Transpose,
  Retune Speed, Humanize y Output.
- Paso de audio y control de ganancia de salida.
- Un estimador básico de frecuencia fundamental por autocorrelación,
  todavía no conectado al editor ni al procesamiento.

Todavía NO incluye:
- Editor visual de notas arrastrables.
- Importación/exportación WAV desde la ventana del plugin.
- Corrección de afinación ni pitch shifting de calidad.
- Separación/unión de notas, edición temporal ni formantes.
- Sincronización de audio editado de vuelta al DAW.

No se debe anunciar como funcional un afinador hasta implementar y probar esas partes.

## Requisitos

- Windows 10/11 de 64 bits.
- Visual Studio 2022 con Desktop development with C++.
- CMake 3.22 o superior.
- Conexión a Internet durante la primera configuración para descargar JUCE.

## Compilar en Windows

Desde la carpeta que contiene `CMakeLists.txt`:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Los artefactos se generan dentro de `build/LaPautaTune_artefacts/Release/`.
La ubicación exacta puede variar según la versión de CMake y Visual Studio.

## Próxima etapa de desarrollo

1. Implementar importación de WAV y reproducción dentro del editor.
2. Detectar F0 por ventanas y convertirla en segmentos de notas.
3. Dibujar notas sobre una rejilla de piano roll y permitir arrastrarlas.
4. Implementar corrección de pitch con preservación de duración y formantes.
5. Exportar WAV procesado y validar la reinserción en REAPER.


## Requisitos de edición definidos para V1

- Herramienta cuchilla para dividir segmentos/notas vocales: pendiente de implementación.
- Selector de tonalidad y escala: controles KEY y SCALE incluidos en esta base.
- Selector de notas individuales del beat y corrección real hacia esas notas: pendiente de implementación.
