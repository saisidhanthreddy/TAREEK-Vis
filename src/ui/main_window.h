#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <QDir>
#include <memory>

#include "map_widget.h"
#include "timeline_widget.h"
#include "info_panel.h"
#include "data/network_index.h"
#include "data/vehicle_index.h"
#include "parsers/preprocessor.h"
#include "parsers/counts_parser.h"
#include "core/crs_transform.h"
#include "core/video_recorder.h"
#include "screenshot_exporter.h"

namespace simvis {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    void openFolder();
    void openFiles();
    // Scan a directory for network/events/transit files and load them.
    // Used by Open Folder, Recent Folders, and the CLI folder argument.
    void loadFolder(const QString& dirPath);
    void loadNetworkFile(const QString& path);
    void loadEventsFile(const QString& path);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPreprocessingProgress(int percent, const QString& message);
    void onPreprocessingComplete(bool success);
    void onSimulationTimeChanged(float time);
    void onActiveVehicleCountChanged(size_t count);
    void updateStatusBar();

    // View actions
    void onShowNodesToggled(bool checked);
    void onShowLinksToggled(bool checked);
    void onShowVehiclesToggled(bool checked);
    void onFitToNetwork();

    // Vehicle options
    void onVehicleShapeCircle();
    void onVehicleShapeTriangle();
    void onVehicleShapeRectangle();
    void onVehicleShapeDiamond();
    void onVehicleShapeAuto();
    void onVehicleSizeSmall();
    void onVehicleSizeMedium();
    void onVehicleSizeLarge();
    void onVehicleColorSpeed();
    void onVehicleColorMode();

    // Transit options
    void onShowBusRoutesToggled(bool checked);
    void onShowBusStopsToggled(bool checked);
    void onShowTramRoutesToggled(bool checked);
    void onShowRailRoutesToggled(bool checked);
    void onRouteThicknessThin();
    void onRouteThicknessMedium();
    void onRouteThicknessThick();

    // Info panel
    void onVehicleTrackingChanged(uint32_t vehicleId);
    void onShowPersonRouteToggled(bool show);
    void onTripClicked(int tripIndex);
    void onTransitStopClicked(int stopIndex);
    void onTransitRouteClicked(uint32_t lineId);
    void onInfoPanelStopClicked(uint32_t stopId);
    void onInfoPanelRouteClicked(uint32_t lineId);
    void onInfoPanelPanTo(float x, float y);

    // Video recording
    void onStartRecording();
    void onStopRecording();
    void onRecordingStarted();
    void onRecordingStopped(bool success, const QString& message);
    void onRecordingProgress(int percent);

    // Counts
    void onLoadCounts();
    void onShowCountsToggled(bool checked);
    void onCountLinkClicked(uint32_t linkId);

    // Network link details
    void onNetworkLinkClicked(uint32_t linkId);

    // Export
    void onExportPdf();
    void onExportPng();

private:
    void setupUi();
    void setupMenus();
    void setupStatusBar();

    // Common tail of all open flows: set paths, title, cache dir, then load
    // from cache or start preprocessing. Transit path may be empty.
    void loadFromPaths(const QString& networkPath, const QString& eventsPath,
                       const QString& transitPath);
    // Find the file for one role (e.g. "network") in dir. Matches
    // *<role>*.xml / *<role>*.xml.gz case-insensitively, prefers the
    // standard MATSim "output_" prefix, asks the user if still ambiguous.
    // Returns empty string if no candidate (or user cancelled the chooser).
    QString resolveInputFile(const QDir& dir, const QString& role);

    // Recent folders (persisted via QSettings, newest first, max 5)
    void addRecentFolder(const QString& dirPath);
    void updateRecentFoldersMenu();

    // Assemble VehicleInfo for the tracked vehicle and refresh the panel.
    // No-op if no vehicle is tracked.
    void refreshVehicleInfoPanel();

    // Build the tracked person's route geometry (vehicle legs follow the
    // network via trajectory segments; teleported legs are straight lines).
    // tripIndex = -1 builds the full day; >= 0 builds only that trip.
    std::vector<PersonRouteRenderer::Segment> buildPersonRouteSegments(
        uint32_t personId, int tripIndex = -1) const;

    // Activity icon markers for the trips included in the overlay
    std::vector<MapWidget::ActivityMarker> buildActivityMarkers(
        uint32_t personId, int tripIndex = -1) const;

    void loadBinaryFiles();
    void startPreprocessing();
    void applyTransitData();
    void loadCachedCRS(const QString& crsPath);
    void parseTransitOnly();

    // UI components
    MapWidget* mapWidget_;
    TimelineWidget* timelineWidget_;
    InfoPanel* infoPanel_;
    QLabel* statusLabel_;
    QLabel* vehicleCountLabel_;
    QLabel* zoomLabel_;

    // Data managers
    std::unique_ptr<NetworkIndex> networkIndex_;
    std::unique_ptr<VehicleIndex> vehicleIndex_;
    std::unique_ptr<Preprocessor> preprocessor_;

    // Transit data (owned by MainWindow after preprocessing)
    std::unique_ptr<TransitScheduleParser::Result> transitData_;

    // Counts data
    std::unique_ptr<CountsData> countsData_;

    // Video recording
    std::unique_ptr<VideoRecorder> videoRecorder_;
    QAction* startRecordingAction_;
    QAction* stopRecordingAction_;

    // File paths
    QString networkFilePath_;
    QString eventsFilePath_;
    QString transitSchedulePath_;
    QString cacheDirectory_;

    // Menu actions
    QAction* showNodesAction_;
    QAction* showLinksAction_;
    QAction* showVehiclesAction_;
    QAction* showCarsAction_;
    QAction* showBusVehiclesAction_;
    QAction* showTramVehiclesAction_;
    QAction* showRailVehiclesAction_;

    // Vehicle shape/size/color actions
    QAction* vehicleShapeCircleAction_;
    QAction* vehicleShapeTriangleAction_;
    QAction* vehicleShapeRectangleAction_;
    QAction* vehicleShapeDiamondAction_;
    QAction* vehicleShapeAutoAction_;
    QAction* vehicleSizeSmallAction_;
    QAction* vehicleSizeMediumAction_;
    QAction* vehicleSizeLargeAction_;
    QAction* vehicleColorSpeedAction_;
    QAction* vehicleColorModeAction_;

    // Transit actions
    QAction* showBusRoutesAction_;
    QAction* showBusStopsAction_;
    QAction* showTramRoutesAction_;
    QAction* showRailRoutesAction_;
    QAction* routeThicknessThinAction_;
    QAction* routeThicknessMediumAction_;
    QAction* routeThicknessThickAction_;

    // Info panel action
    QAction* showInfoPanelAction_;

    // Counts actions
    QAction* loadCountsAction_;
    QAction* showCountsAction_;

    // Recent folders submenu (rebuilt from QSettings on every change)
    QMenu* recentFoldersMenu_ = nullptr;

    // Background map actions
    QAction* showBackgroundMapAction_;
    QAction* tileSourceOsmAction_;
    QAction* tileSourceSatAction_;
    QAction* tileSourceTopoAction_;

    // CRS info
    CRSInfo networkCrs_;

    // Throttle for live vehicle-info panel refresh (sim seconds of last update)
    float lastVehicleInfoRefreshTime_ = -1.0f;

    // Person whose trips/route the panel currently shows (personId + 1, 0 = none)
    // and the vehicle the panel describes. Both survive tracking loss (clicking
    // empty map to pan) so the route overlay and trip clicks keep working until
    // a different vehicle is selected.
    uint32_t routePersonPlus1_ = 0;
    uint32_t infoVehicleId_ = 0;
};

} // namespace simvis
