/*
Copyright 2009 Will Stephenson <wstephenson@kde.org>
Copyright 2013 Lukas Tinkl <ltinkl@redhat.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLASMA_NM_PPTP_H
#define PLASMA_NM_PPTP_H

#include "vpnuiplugin.h"

#include <QVariant>

class PptpUiPlugin : public VpnUiPlugin
{
Q_OBJECT
public:
    explicit PptpUiPlugin(QObject * parent = 0, const QVariantList& = QVariantList());
    virtual ~PptpUiPlugin();
    virtual SettingWidget * widget(const NetworkManager::VpnSetting::Ptr &setting, QWidget * parent = 0);
    virtual SettingWidget * askUser(const NetworkManager::VpnSetting::Ptr &setting, QWidget * parent = 0);
#if 0
    QString suggestedFileName(Knm::Connection *connection) const;
    QString supportedFileExtensions() const;
    QVariantList importConnectionSettings(const QString &fileName);
    bool exportConnectionSettings(Knm::Connection * connection, const QString &fileName);
#endif
};

#endif //  PLASMA_NM_PPTP_H
