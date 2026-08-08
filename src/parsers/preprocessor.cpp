#include "preprocessor.h"
#include "data/vehicle_index.h"
#include "core/logger.h"
#include <QDir>
#include <QFileInfo>
#include <filesystem>

namespace simvis {

Preprocessor::Preprocessor(QObject* parent)
    : QObject(parent)
{
}

Preprocessor::~Preprocessor() = default;

void Preprocessor::setNetworkFile(const QString& path) {
    networkXmlPath_ = path;
}

void Preprocessor::setEventsFile(const QString& path) {
    eventsXmlPath_ = path;
}

void Preprocessor::setTransitScheduleFile(const QString& path) {
    transitSchedulePath_ = path;
}

void Preprocessor::setOutputDirectory(const QString& path) {
    outputDir_ = path;

    // Create directory if it doesn't exist
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

bool Preprocessor::hasCachedFiles() const {
    return QFileInfo::exists(networkBinaryPath()) &&
           QFileInfo::exists(vehicleIndexPath());
}

QString Preprocessor::networkBinaryPath() const {
    if (outputDir_.isEmpty() || networkXmlPath_.isEmpty()) return QString();

    QFileInfo info(networkXmlPath_);
    QString baseName = info.completeBaseName();  // removes .gz
    if (baseName.endsWith(".xml")) {
        baseName = baseName.left(baseName.length() - 4);
    }
    return outputDir_ + "/" + baseName + ".bin";
}

QString Preprocessor::vehicleIndexPath() const {
    if (outputDir_.isEmpty() || eventsXmlPath_.isEmpty()) return QString();

    QFileInfo info(eventsXmlPath_);
    QString baseName = info.completeBaseName();
    if (baseName.endsWith(".xml")) {
        baseName = baseName.left(baseName.length() - 4);
    }
    return outputDir_ + "/" + baseName + ".vidx";
}

void Preprocessor::processNetwork() {
    cancelled_ = false;
    LOG_INFO(QString("Preprocessor: parsing network XML: %1").arg(networkXmlPath_));

    emit progressChanged(0, "Starting network preprocessing...");

    auto progressCb = [this](size_t current, size_t total, const std::string& msg) {
        if (cancelled_) return;

        int percent = (total > 0) ? static_cast<int>(100 * current / total) : 0;
        // Network is 0-50% of total progress
        emit progressChanged(percent / 2, QString::fromStdString(msg));
    };

    auto result = NetworkParser::parseGzipFile(
        networkXmlPath_.toStdString(),
        progressCb
    );

    if (cancelled_) {
        emit networkCompleted(false, "Cancelled");
        return;
    }

    if (!result.success) {
        LOG_ERROR(QString("Preprocessor: network parse failed - %1")
            .arg(QString::fromStdString(result.errorMessage)));
        emit networkCompleted(false, QString::fromStdString(result.errorMessage));
        return;
    }

    // Save result for event parsing
    networkResult_ = std::make_unique<NetworkParser::Result>(std::move(result));

    // Write binary file
    emit progressChanged(45, "Writing network binary file...");

    bool writeSuccess = NetworkParser::writeBinary(
        networkBinaryPath().toStdString(),
        *networkResult_
    );

    if (writeSuccess) {
        QString msg = QString("Network: %1 nodes, %2 links")
            .arg(networkResult_->nodes.size())
            .arg(networkResult_->links.size());
        LOG_INFO(QString("Preprocessor: network done - %1").arg(msg));
        emit networkCompleted(true, msg);
    } else {
        LOG_ERROR("Preprocessor: failed to write network .bin");
        emit networkCompleted(false, "Failed to write binary file");
    }
}

void Preprocessor::processEvents() {
    if (!networkResult_) {
        emit eventsCompleted(false, "Network must be processed first");
        return;
    }

    cancelled_ = false;
    LOG_INFO(QString("Preprocessor: parsing events XML: %1").arg(eventsXmlPath_));

    emit progressChanged(50, "Starting events preprocessing...");

    auto progressCb = [this](size_t current, size_t total, const std::string& msg) {
        if (cancelled_) return;

        int percent = (total > 0) ? static_cast<int>(100 * current / total) : 0;
        // Events is 50-90% of total progress
        emit progressChanged(50 + percent * 40 / 100, QString::fromStdString(msg));
    };

    // Single-pass: parse XML and build vehicle index simultaneously
    auto streamResult = EventParser::streamProcessEvents(
        eventsXmlPath_.toStdString(),
        networkResult_->linkIds,
        progressCb
    );

    if (cancelled_) {
        emit eventsCompleted(false, "Cancelled");
        return;
    }

    if (!streamResult.success) {
        LOG_ERROR(QString("Preprocessor: events parse failed - %1")
            .arg(QString::fromStdString(streamResult.errorMessage)));
        emit eventsCompleted(false, QString::fromStdString(streamResult.errorMessage));
        return;
    }

    // Store result - defer writing until after transit parsing (for mode cross-reference)
    eventsResult_ = std::make_unique<EventParser::StreamResult>(std::move(streamResult));

    size_t totalSegments = 0;
    for (const auto& t : eventsResult_->vehicleIndex.vehicles_)
        totalSegments += t.segments.size();

    QString msg = QString("Events: %1 vehicles, %2 total segments (time: %3s - %4s)")
        .arg(eventsResult_->vehicleIndex.vehicleCount())
        .arg(totalSegments)
        .arg(VehicleIndex::toSeconds(eventsResult_->vehicleIndex.minTime()), 0, 'f', 1)
        .arg(VehicleIndex::toSeconds(eventsResult_->vehicleIndex.maxTime()), 0, 'f', 1);
    LOG_INFO(QString("Preprocessor: events done - %1").arg(msg));
    emit eventsCompleted(true, msg);
}

void Preprocessor::processTransitSchedule() {
    if (!networkResult_) {
        emit transitCompleted(false, "Network must be processed first");
        return;
    }

    if (transitSchedulePath_.isEmpty()) {
        // Transit is optional - not an error
        emit transitCompleted(true, "No transit schedule file provided");
        return;
    }

    LOG_INFO(QString("Preprocessor: parsing transit schedule: %1").arg(transitSchedulePath_));
    emit progressChanged(90, "Parsing transit schedule...");

    auto progressCb = [this](size_t current, size_t total, const std::string& msg) {
        if (cancelled_) return;
        int percent = (total > 0) ? static_cast<int>(100 * current / total) : 0;
        // Transit is 90-100% of total progress
        emit progressChanged(90 + percent / 10, QString::fromStdString(msg));
    };

    // Detect gzip vs plain XML
    bool isGzip = transitSchedulePath_.endsWith(".gz", Qt::CaseInsensitive);

    TransitScheduleParser::Result result;
    if (isGzip) {
        result = TransitScheduleParser::parseGzipFile(
            transitSchedulePath_.toStdString(),
            networkResult_->linkIds,
            progressCb
        );
    } else {
        result = TransitScheduleParser::parse(
            transitSchedulePath_.toStdString(),
            networkResult_->linkIds,
            progressCb
        );
    }

    if (cancelled_) {
        emit transitCompleted(false, "Cancelled");
        return;
    }

    if (!result.success) {
        LOG_ERROR(QString("Preprocessor: transit parse failed - %1")
            .arg(QString::fromStdString(result.errorMessage)));
        emit transitCompleted(false, QString::fromStdString(result.errorMessage));
        return;
    }

    QString msg = QString("Transit: %1 stops, %2 lines, %3 vehicle mappings")
        .arg(result.stops.size())
        .arg(result.lines.size())
        .arg(result.vehicleToMode.size());

    LOG_INFO(QString("Preprocessor: transit done - %1").arg(msg));
    transitResult_ = std::make_unique<TransitScheduleParser::Result>(std::move(result));
    emit transitCompleted(true, msg);
}

void Preprocessor::writeVehicleIndex() {
    if (!eventsResult_) {
        return;
    }

    // Cross-reference transit vehicle modes into VehicleIndex
    if (transitResult_ && transitResult_->success) {
        auto& vidx = eventsResult_->vehicleIndex;
        const auto& vehicleToMode = transitResult_->vehicleToMode;
        const auto& vehicleIds = eventsResult_->vehicleIds;

        size_t classified = 0;
        for (size_t i = 0; i < vehicleIds.size(); ++i) {
            const auto& vehicleName = vehicleIds.strings()[i];
            auto it = vehicleToMode.find(vehicleName);
            if (it != vehicleToMode.end()) {
                vidx.vehicleModes_[i] = static_cast<uint8_t>(it->second);
                ++classified;
            }
        }

        LOG_INFO(QString("Preprocessor: classified %1 transit vehicles").arg(classified));
        emit progressChanged(97, QString("Classified %1 transit vehicles").arg(classified));
    }

    emit progressChanged(98, "Writing vehicle index...");

    // Move the events-side ID interners into the index so they are serialized
    // into the .vidx (v3 string tables). Must happen before eventsResult_.reset().
    {
        auto& vidx = eventsResult_->vehicleIndex;
        vidx.vehicleIdStrings_ = std::move(eventsResult_->vehicleIds);
        vidx.personIdStrings_ = std::move(eventsResult_->personIds);
    }

    bool writeSuccess = VehicleIndex::writeFile(
        vehicleIndexPath(), eventsResult_->vehicleIndex);

    if (!writeSuccess) {
        LOG_ERROR("Preprocessor: failed to write .vidx");
        emit eventsCompleted(false, "Failed to write vehicle index file");
    }

    eventsResult_.reset();
}

void Preprocessor::processAll() {
    processNetwork();

    if (cancelled_) {
        emit allCompleted(false);
        return;
    }

    if (!networkResult_ || networkResult_->nodes.empty()) {
        emit allCompleted(false);
        return;
    }

    processEvents();

    if (cancelled_) {
        emit allCompleted(false);
        return;
    }

    // Parse transit schedule (optional - won't fail if not provided)
    processTransitSchedule();

    if (cancelled_) {
        emit allCompleted(false);
        return;
    }

    // Cross-reference transit modes and write .vidx (must be after both events + transit)
    writeVehicleIndex();

    emit progressChanged(100, "Preprocessing complete");
    emit allCompleted(!cancelled_);
}

void Preprocessor::cancel() {
    cancelled_ = true;
}

} // namespace simvis
