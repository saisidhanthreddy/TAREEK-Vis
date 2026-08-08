#pragma once

#include "parsers/counts_parser.h"
#include <QDialog>
#include <QWidget>

namespace simvis {

// Custom widget that paints the bar chart
class CountsChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit CountsChartWidget(const CountData& data, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const CountData& countData_;
};

// Dialog window showing the 24-hour volume comparison chart
class CountsChartDialog : public QDialog {
    Q_OBJECT
public:
    CountsChartDialog(const CountData& data,
                      const std::string& linkOriginalId,
                      QWidget* parent = nullptr);
};

} // namespace simvis
