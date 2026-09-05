#include "hourly_volume_chart.h"
#include "panel_style.h"

#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QToolTip>
#include <algorithm>

namespace simvis {
namespace {

// Room for the y-axis numbers and the axis caption. The hour-label band is
// measured at paint time, since it depends on whether the labels stagger.
constexpr int kMarginLeft = 34;
constexpr int kMarginRight = 6;
constexpr int kMarginTop = 8;
constexpr int kAxisCaptionBand = 14; // "Hour of Day"

constexpr int kYTicks = 4;

// Round the axis top up to something a reader can divide by eye, matching the
// steps CountsChartWidget uses.
uint32_t niceAxisMax(uint32_t peak) {
    if (peak == 0) return 10;
    if (peak <= 100) return ((peak / 10) + 1) * 10;
    if (peak <= 1000) return ((peak / 100) + 1) * 100;
    if (peak <= 10000) return ((peak / 1000) + 1) * 1000;
    return ((peak / 5000) + 1) * 5000;
}

} // namespace

HourlyVolumeChart::HourlyVolumeChart(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(180);
}

void HourlyVolumeChart::setVolumes(const std::vector<uint32_t>& volumes) {
    if (volumes.size() != 24) {
        volumes_.clear();
        maxVolume_ = 0;
    } else {
        volumes_ = volumes;
        maxVolume_ = *std::max_element(volumes_.begin(), volumes_.end());
    }
    hoverHour_ = -1;
    update();
}

QSize HourlyVolumeChart::sizeHint() const {
    return QSize(240, 200);
}

// Hour labels stay horizontal. Where 24 of them will not fit side by side they
// drop onto two alternating rows rather than turning on their side, which keeps
// every hour readable without asking anyone to tilt their head.
static bool needsStaggeredLabels(const QFontMetrics& fm, double slotW) {
    return fm.horizontalAdvance(QStringLiteral("24")) + 3 > slotW;
}

int HourlyVolumeChart::hourAt(int x) const {
    const int plotW = width() - kMarginLeft - kMarginRight;
    if (volumes_.size() != 24 || plotW <= 0) return -1;
    if (x < kMarginLeft || x >= kMarginLeft + plotW) return -1;
    const int hour = (x - kMarginLeft) * 24 / plotW;
    return std::clamp(hour, 0, 23);
}

void HourlyVolumeChart::paintEvent(QPaintEvent* /*event*/) {
    if (volumes_.size() != 24) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont tickFont = font();
    tickFont.setPointSize(7);
    p.setFont(tickFont);

    const QFontMetrics fm(tickFont);
    const int plotW = width() - kMarginLeft - kMarginRight;
    const double slotW = static_cast<double>(plotW) / 24.0;
    const bool stagger = needsStaggeredLabels(fm, slotW);
    const int labelRowH = fm.height();
    const int tickLen = 3;
    const int hourLabelBand = (stagger ? 2 : 1) * labelRowH + tickLen + 2;

    const int plotH = height() - kMarginTop - hourLabelBand - kAxisCaptionBand;
    if (plotW < 60 || plotH < 40) return;

    const int baseline = kMarginTop + plotH;
    const uint32_t axisMax = niceAxisMax(maxVolume_);

    // Horizontal grid with its value, so bar heights can be read off
    for (int i = 0; i <= kYTicks; ++i) {
        const int y = baseline - i * plotH / kYTicks;
        p.setPen(QPen(QColor(255, 255, 255, 30), 1));
        p.drawLine(kMarginLeft, y, kMarginLeft + plotW, y);

        p.setPen(QColor(150, 150, 155));
        p.drawText(0, y - 7, kMarginLeft - 4, 14,
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(static_cast<uint32_t>(i) * axisMax / kYTicks));
    }

    const double barW = std::max(2.0, slotW * 0.74);

    const QColor barColor(panelstyle::kAccent);
    QColor hoverColor = barColor.lighter(135);

    for (int h = 0; h < 24; ++h) {
        const double slotX = kMarginLeft + h * slotW;
        const double barX = slotX + (slotW - barW) / 2.0;

        // Every hour keeps its slot. An hour with no traffic simply draws no
        // bar, which is what an empty hour should look like.
        if (volumes_[h] > 0) {
            const double barH =
                static_cast<double>(volumes_[h]) / axisMax * plotH;
            p.fillRect(QRectF(barX, baseline - barH, barW, barH),
                       h == hoverHour_ ? hoverColor : barColor);
        }

        // Hour label, always upright and horizontal. When the slots are too
        // narrow for 24 side by side, alternate hours drop to a second row
        // instead of the numbers being turned on their side.
        const int row = stagger ? (h % 2) : 0;
        const bool hot = (h == hoverHour_);
        const double centreX = slotX + slotW / 2.0;

        // A tick per hour ties each number to its own slot, which matters most
        // when the numbers alternate between two rows.
        p.setPen(QPen(hot ? QColor(220, 220, 226) : QColor(120, 120, 126), 1));
        p.drawLine(QPointF(centreX, baseline + 1),
                   QPointF(centreX, baseline + 1 + tickLen));

        p.setPen(hot ? QColor(235, 235, 240) : QColor(150, 150, 155));
        p.drawText(QRectF(centreX - slotW, baseline + tickLen + 2 + row * labelRowH,
                          slotW * 2.0, labelRowH),
                   Qt::AlignHCenter | Qt::AlignVCenter, QString::number(h + 1));
    }

    // Axis line and caption
    p.setPen(QPen(QColor(160, 160, 165), 1));
    p.drawLine(kMarginLeft, baseline, kMarginLeft + plotW, baseline);

    QFont captionFont = font();
    captionFont.setPointSize(8);
    p.setFont(captionFont);
    p.setPen(QColor(170, 170, 175));
    p.drawText(kMarginLeft, baseline + hourLabelBand, plotW, kAxisCaptionBand,
               Qt::AlignHCenter | Qt::AlignVCenter, tr("Hour of Day"));
}

void HourlyVolumeChart::mouseMoveEvent(QMouseEvent* event) {
    const int hour = hourAt(event->pos().x());
    if (hour != hoverHour_) {
        hoverHour_ = hour;
        update();
    }
    if (hour >= 0) {
        // Spell out the clock range, so the hour numbering cannot be misread
        QToolTip::showText(
            event->globalPosition().toPoint(),
            tr("Hour %1  (%2:00-%3:00)\n%4 vehicles")
                .arg(hour + 1)
                .arg(hour, 2, 10, QChar('0'))
                .arg((hour + 1) % 24, 2, 10, QChar('0'))
                .arg(volumes_[hour]),
            this);
    } else {
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void HourlyVolumeChart::leaveEvent(QEvent* event) {
    if (hoverHour_ != -1) {
        hoverHour_ = -1;
        update();
    }
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

} // namespace simvis
