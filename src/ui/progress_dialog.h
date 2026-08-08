#pragma once

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace simvis {

// Progress dialog for preprocessing files
class ProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget* parent = nullptr);
    ~ProgressDialog() override;

    void setTitle(const QString& title);
    void setProgress(int percent, const QString& message);
    void setCompleted(bool success, const QString& message);

signals:
    void cancelRequested();

private:
    QLabel* titleLabel_;
    QProgressBar* progressBar_;
    QLabel* messageLabel_;
    QPushButton* cancelButton_;
    QPushButton* closeButton_;
};

} // namespace simvis
