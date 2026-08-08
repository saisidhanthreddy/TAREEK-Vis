#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>

namespace simvis {

// Timeline control widget with slider, play/pause, and speed control
class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget() override;

    // Set time range (in seconds)
    void setTimeRange(float minTime, float maxTime);

    // Set current time
    void setCurrentTime(float time);
    float currentTime() const;

    // Playback state
    bool isPlaying() const { return isPlaying_; }
    double playbackSpeed() const;

signals:
    void timeChanged(float time);
    void playingChanged(bool playing);
    void playbackSpeedChanged(double speed);

public slots:
    void setPlaying(bool playing);
    void togglePlayPause();

private slots:
    void onSliderMoved(int value);
    void onSliderPressed();
    void onSliderReleased();
    void onSpeedChanged(int index);

private:
    void updateTimeLabel();
    QString formatTime(float seconds) const;

    QSlider* slider_;
    QLabel* timeLabel_;
    QLabel* durationLabel_;
    QPushButton* playButton_;
    QComboBox* speedCombo_;

    float minTime_ = 0.0f;
    float maxTime_ = 86400.0f;  // 24 hours default
    float currentTime_ = 0.0f;
    bool isPlaying_ = false;
    bool wasPlayingBeforeDrag_ = false;
    bool isDragging_ = false;  // Track if user is currently dragging the slider
};

// Performance tuning notes:
// - The slider only emits timeChanged on release to avoid loading events during drag
// - Time label updates in real-time for user feedback during dragging

} // namespace simvis
