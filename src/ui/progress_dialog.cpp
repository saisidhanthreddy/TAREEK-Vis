#include "progress_dialog.h"

namespace simvis {

ProgressDialog::ProgressDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Processing Files");
    setMinimumWidth(400);
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    titleLabel_ = new QLabel("Preprocessing simulation files...", this);
    titleLabel_->setStyleSheet("font-weight: bold; font-size: 14px;");

    progressBar_ = new QProgressBar(this);
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);

    messageLabel_ = new QLabel("Initializing...", this);
    messageLabel_->setWordWrap(true);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    cancelButton_ = new QPushButton("Cancel", this);
    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        emit cancelRequested();
        cancelButton_->setEnabled(false);
        cancelButton_->setText("Cancelling...");
    });

    closeButton_ = new QPushButton("Close", this);
    closeButton_->setEnabled(false);
    closeButton_->setVisible(false);
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(cancelButton_);
    buttonLayout->addWidget(closeButton_);

    layout->addWidget(titleLabel_);
    layout->addWidget(progressBar_);
    layout->addWidget(messageLabel_);
    layout->addLayout(buttonLayout);

    setLayout(layout);
}

ProgressDialog::~ProgressDialog() = default;

void ProgressDialog::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

void ProgressDialog::setProgress(int percent, const QString& message) {
    progressBar_->setValue(percent);
    messageLabel_->setText(message);
}

void ProgressDialog::setCompleted(bool success, const QString& message) {
    progressBar_->setValue(100);

    if (success) {
        messageLabel_->setText(message);
        titleLabel_->setText("Processing Complete");
    } else {
        messageLabel_->setText("Error: " + message);
        titleLabel_->setText("Processing Failed");
    }

    cancelButton_->setVisible(false);
    closeButton_->setEnabled(true);
    closeButton_->setVisible(true);
}

} // namespace simvis
