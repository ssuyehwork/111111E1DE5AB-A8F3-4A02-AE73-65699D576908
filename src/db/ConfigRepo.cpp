#include "ConfigRepo.h"
#include "Database.h"
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>

namespace ArcMeta {

bool ConfigRepo::setConfig(const QString& key, const QString& value) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO sync_state (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

QString ConfigRepo::getConfig(const QString& key, const QString& defaultValue) {
    QSqlDatabase db = Database::instance().getDb();
    QSqlQuery query(db);
    query.prepare("SELECT value FROM sync_state WHERE key = ?");
    query.addBindValue(key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

void ConfigRepo::savePanelWidths(const QList<int>& widths) {
    QJsonArray arr;
    for (int w : widths) arr.append(w);
    setConfig("panel_widths", QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QList<int> ConfigRepo::loadPanelWidths() {
    QString val = getConfig("panel_widths");
    QList<int> result;
    if (!val.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
        for (const auto& item : doc.array()) {
            result.append(item.toInt());
        }
    }
    return result;
}

} // namespace ArcMeta
