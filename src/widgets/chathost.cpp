// chathost.cpp - default implementations of ChatHost.
// Copyright (C) 2016 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026
#include <cutechat/chathost.h>

#include <QJsonDocument>
#include <QMessageBox>

bool ChatHost::approveToolCall(const ToolSpec& spec, const QJsonObject& args, QWidget* parent)
{
    const QString text = QObject::tr("The assistant wants to run '%1'.\n\n%2\n\nAllow?")
                             .arg(spec.name, QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)));
    return QMessageBox::question(parent, uiTexts().approvalTitle, text) == QMessageBox::Yes;
}
