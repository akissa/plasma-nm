/*
    Copyright 2013 Lukas Tinkl <ltinkl@redhat.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "wimaxwidget.h"
#include "ui_wimax.h"
#include "uiutils.h"

#include <NetworkManagerQt/WimaxSetting>

#include <KLocalizedString>

WimaxWidget::WimaxWidget(const NetworkManager::Setting::Ptr &setting, QWidget* parent, Qt::WindowFlags f):
    SettingWidget(setting, parent, f),
    m_ui(new Ui::WimaxWidget)
{
    m_ui->setupUi(this);

    if (setting)
        loadConfig(setting);
}

WimaxWidget::~WimaxWidget()
{
}

void WimaxWidget::loadConfig(const NetworkManager::Setting::Ptr &setting)
{
    NetworkManager::WimaxSetting::Ptr wimaxSetting = setting.staticCast<NetworkManager::WimaxSetting>();

    m_ui->networkName->setText(wimaxSetting->networkName());
    m_ui->macAddress->init(NetworkManager::Device::Wimax, UiUtils::macAddressAsString(wimaxSetting->macAddress()));
}

QVariantMap WimaxWidget::setting(bool agentOwned) const
{
    Q_UNUSED(agentOwned);

    NetworkManager::WimaxSetting wimaxSetting;

    wimaxSetting.setNetworkName(m_ui->networkName->text());
    wimaxSetting.setMacAddress(UiUtils::macAddressFromString(m_ui->macAddress->hwAddress()));

    return wimaxSetting.toMap();
}