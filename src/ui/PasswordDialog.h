#ifndef PASSWORD_DIALOG_H
#define PASSWORD_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

namespace ArcMeta {

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("加密验证");
        setFixedSize(300, 150);
        auto* layout = new QVBoxLayout(this);

        layout->addWidget(new QLabel("请输入密码以访问加密文件:"));

        m_passwordEdit = new QLineEdit(this);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        layout->addWidget(m_passwordEdit);

        auto* btnBox = new QHBoxLayout();
        auto* btnOk = new QPushButton("确认", this);
        auto* btnCancel = new QPushButton("取消", this);
        btnBox->addWidget(btnOk);
        btnBox->addWidget(btnCancel);
        layout->addLayout(btnBox);

        connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    QString getPassword() const { return m_passwordEdit->text(); }

private:
    QLineEdit* m_passwordEdit;
};

} // namespace ArcMeta

#endif // PASSWORD_DIALOG_H
