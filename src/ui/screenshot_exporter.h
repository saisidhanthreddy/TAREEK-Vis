#pragma once

#include <QWidget>
#include <QImage>
#include <QString>

namespace simvis {

class MapWidget;

class ScreenshotExporter {
public:
    // Export current map view as a high-res PNG
    // scaleFactor: 1=native, 2=2x, 4=4x resolution
    static bool exportPng(MapWidget* mapWidget, QWidget* parent, int scaleFactor = 2);

    // Export current map view as a PDF with embedded high-res raster
    // scaleFactor: multiplier for the raster resolution embedded in the PDF
    static bool exportPdf(MapWidget* mapWidget, QWidget* parent, int scaleFactor = 4);

private:
    static QImage captureScaled(MapWidget* mapWidget, int scaleFactor);
};

} // namespace simvis
