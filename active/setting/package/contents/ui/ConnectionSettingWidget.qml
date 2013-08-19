/*
    Copyright 2013 Jan Grulich <jgrulich@redhat.com>

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

import QtQuick 1.1
import org.kde.plasma.components 0.1 as PlasmaComponents
import org.kde.plasma.extras 0.1 as PlasmaExtras

Item {
    id: connectionSettingWidget;

    PlasmaExtras.Heading {
        id: connectionSettingLabel;

        anchors {
            right: parent.horizontalCenter;
            rightMargin: 12;
        }
        text: i18n("Connection configuration");
        level: 3;
    }

    Item {
        id: connectionName;

        anchors {
            left: parent.left;
            right: parent.right;
            top: connectionSettingLabel.bottom;
            topMargin: 24;
        }

        PlasmaComponents.Label {
            id: connectionNameLabel;

            anchors {
                right: parent.horizontalCenter;
                verticalCenter: parent.verticalCenter;
                rightMargin: 12;
            }
            text: i18n("Connection name:");
        }

        PlasmaComponents.TextField {
            id: connectionNameInput;

            width: 200;
            anchors {
                left: parent.horizontalCenter;
                verticalCenter: parent.verticalCenter;
            }
        }
    }

    Item {
        id: automaticallyConnect;

        anchors {
            left: parent.left;
            right: parent.right;
            top: connectionName.bottom;
            topMargin: 36;
        }

        PlasmaComponents.Label {
            id: automaticallyConnectLabel;

            anchors {
                right: parent.horizontalCenter;
                verticalCenter: parent.verticalCenter;
                rightMargin: 12;
            }
            text: i18n("Automatically connect:");
        }

        PlasmaComponents.Switch {
            id: automaticallyConnectSwitch;

            anchors {
                left: parent.horizontalCenter;
                verticalCenter: parent.verticalCenter;
            }
            checked: true;
        }
    }

    function loadSetting(settingMap) {
        connectionNameInput.text = settingMap["name"];
        if (settingMap["autoconnect"] == "true") {
            automaticallyConnectSwitch.checked = true;
        } else {
            automaticallyConnectSwitch.checked = false;
        }
    }

    function getSetting() {
        var settingMap = [];

        settingMap.name = connectionNameInput.text;
        if (automaticallyConnectSwitch.checked) {
            settingMap.autoconnect = "true";
        } else {
            settingMap.autoconnect = "false";
        }

        return settingMap;
    }
}
