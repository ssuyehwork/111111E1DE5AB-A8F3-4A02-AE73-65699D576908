#ifndef PASSWORD_DIALOG_H
#define PASSWORD_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace ArcMeta {

class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordDialog(bool isSetting, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(isSetting ? "设置加密密码" : "验证密码");
        setFixedWidth(300);

        auto* layout = new QVBoxLayout(this);

        layout->addWidget(new QLabel(isSetting ? "请设置 8 位以上复杂密码：" : "请输入文件访问密码："));

        m_pass1 = new QLineEdit();
        m_pass1->setEchoMode(QLineEdit::Password);
        layout->addWidget(m_pass1);

        if (isSetting) {
            m_pass2 = new QLineEdit();
            m_pass2->setPlaceholderText("再次输入以确认");
            m_pass2->setEchoMode(QLineEdit::Password);
            layout->addWidget(m_pass2);
        }

        auto* btnBox = new QHBoxLayout();
        auto* btnOk = new QPushButton("确认");
        auto* btnCancel = new QPushButton("取消");
        btnBox->addWidget(btnOk);
        btnBox->addWidget(btnCancel);
        layout->addLayout(btnBox);

        connect(btnOk, &QPushButton::clicked, this, &PasswordDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &PasswordDialog::reject);
    }

    QString password() const { return m_pass1->text(); }

private:
    QLineEdit* m_pass1;
    QLineEdit* m_pass2 = nullptr;
};

} // namespace ArcMeta

#endif // PASSWORD_DIALOG_H
