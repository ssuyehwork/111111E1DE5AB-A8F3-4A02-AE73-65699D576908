#pragma once

#include <QMainWindow>
#include "NavPanel.h"
#include "ContentPanel.h"

namespace ArcMeta {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void initLayout();

    NavPanel* m_navPanel = nullptr;
    ContentPanel* m_contentPanel = nullptr;
};

} // namespace ArcMeta
