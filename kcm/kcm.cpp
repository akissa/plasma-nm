/*
    Copyright 2016 Jan Grulich <jgrulich@redhat.com>

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

#include "kcm.h"

#include "debug.h"
#include "connectioneditordialog.h"
#include "mobileconnectionwizard.h"
#include "uiutils.h"
#include "vpnuiplugin.h"

// KDE
#include <KPluginFactory>
#include <KSharedConfig>
#include <kdeclarative/kdeclarative.h>
#include <KService>
#include <KServiceTypeTrader>

#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/GsmSetting>
#include <NetworkManagerQt/Ipv4Setting>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/Utils>
#include <NetworkManagerQt/VpnSetting>
#include <NetworkManagerQt/WiredSetting>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/WirelessDevice>

// Qt
#include <QFileDialog>
#include <QMenu>
#include <QVBoxLayout>
#include <QTimer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>

K_PLUGIN_FACTORY(KCMNetworkConfigurationFactory, registerPlugin<KCMNetworkmanagement>();)

KCMNetworkmanagement::KCMNetworkmanagement(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_handler(new Handler(this))
    , m_tabWidget(nullptr)
    , m_ui(new Ui::KCMForm)
    , m_quickView(nullptr)
{
    QWidget *mainWidget = new QWidget(this);
    m_ui->setupUi(mainWidget);

    m_quickView = new QQuickView(0);
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(m_quickView->engine());
    kdeclarative.setTranslationDomain(QStringLiteral(TRANSLATION_DOMAIN));
    kdeclarative.setupBindings();

    QWidget *widget = QWidget::createWindowContainer(m_quickView, this);
    widget->setMinimumWidth(300);
    QVBoxLayout *layout = new QVBoxLayout(m_ui->connectionView);
    layout->addWidget(widget);

    m_quickView->rootContext()->setContextProperty("alternateBaseColor", mainWidget->palette().color(QPalette::Active, QPalette::AlternateBase));
    m_quickView->rootContext()->setContextProperty("backgroundColor", mainWidget->palette().color(QPalette::Active, QPalette::Window));
    m_quickView->rootContext()->setContextProperty("baseColor", mainWidget->palette().color(QPalette::Active, QPalette::Base));
    m_quickView->rootContext()->setContextProperty("highlightColor", mainWidget->palette().color(QPalette::Active, QPalette::Highlight));
    m_quickView->rootContext()->setContextProperty("textColor", mainWidget->palette().color(QPalette::Active, QPalette::Text));
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_quickView->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kcm_networkmanagement/qml/main.qml"))));

    QObject *rootItem = m_quickView->rootObject();
    connect(rootItem, SIGNAL(selectedConnectionChanged(QString)), this, SLOT(onSelectedConnectionChanged(QString)));
    connect(rootItem, SIGNAL(requestCreateConnection(int,QString,QString,bool)), this, SLOT(onRequestCreateConnection(int,QString,QString,bool)));
    connect(rootItem, SIGNAL(requestExportConnection(QString)), this, SLOT(onRequestExportConnection(QString)));

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(mainWidget);

    setButtons(Button::Apply);

    // Pre-select currently active primary connection and if there is none then just select
    // the very first connection
    NetworkManager::ActiveConnection::Ptr activeConnection = NetworkManager::primaryConnection();
    if (activeConnection && activeConnection->isValid()) {
        // Also check if the connection type is supported by KCM
        const NetworkManager::ConnectionSettings::ConnectionType type = activeConnection->type();
        if (UiUtils::isConnectionTypeSupported(type)) {
            loadConnectionSettings(activeConnection->connection()->settings());
            QMetaObject::invokeMethod(rootItem, "selectConnection", Q_ARG(QVariant, activeConnection->id()), Q_ARG(QVariant, activeConnection->connection()->path()));
        }
    } else {
        // Select first connection
        NetworkManager::Connection::List connectionList = NetworkManager::listConnections();
        std::sort(connectionList.begin(), connectionList.end(), [] (const NetworkManager::Connection::Ptr &left, const NetworkManager::Connection::Ptr &right)
        {
            const QString leftName = left->settings()->id();
            const UiUtils::SortedConnectionType leftType = UiUtils::connectionTypeToSortedType(left->settings()->connectionType());
            const QDateTime leftDate = left->settings()->timestamp();

            const QString rightName = right->settings()->id();
            const UiUtils::SortedConnectionType rightType = UiUtils::connectionTypeToSortedType(right->settings()->connectionType());
            const QDateTime rightDate = right->settings()->timestamp();

            if (leftType < rightType) {
                return true;
            } else if (leftType > rightType) {
                return false;
            }

            if (leftDate > rightDate) {
                return true;
            } else if (leftDate < rightDate) {
                return false;
            }

            if (QString::localeAwareCompare(leftName, rightName) > 0) {
                return true;
            } else {
                return false;
            }
        });

        Q_FOREACH (const NetworkManager::Connection::Ptr &connection, connectionList) {
            const NetworkManager::ConnectionSettings::ConnectionType type = connection->settings()->connectionType();
            if (UiUtils::isConnectionTypeSupported(type)) {
                loadConnectionSettings(connection->settings());
                QMetaObject::invokeMethod(rootItem, "selectConnection", Q_ARG(QVariant, connection->settings()->id()), Q_ARG(QVariant, connection->path()));
                break;
            }
        }
    }

    connect(NetworkManager::settingsNotifier(), &NetworkManager::SettingsNotifier::connectionAdded, this, &KCMNetworkmanagement::onConnectionAdded, Qt::UniqueConnection);

    // Initialize first scan and then scan every 15 seconds
    m_handler->requestScan();

    m_timer = new QTimer(this);
    m_timer->setInterval(15000);
    connect(m_timer, &QTimer::timeout, [this] () {
        m_handler->requestScan();
        m_timer->start();
    });
    m_timer->start();
}

KCMNetworkmanagement::~KCMNetworkmanagement()
{
    delete m_handler;
    if (m_tabWidget) {
        delete m_tabWidget;
    }
    delete m_quickView;
    delete m_ui;
}

void KCMNetworkmanagement::defaults()
{
    KCModule::defaults();
}

void KCMNetworkmanagement::load()
{
    // If there is no loaded connection do nothing
    if (m_currentConnectionPath.isEmpty()) {
        return;
    }

    NetworkManager::Connection::Ptr connection = NetworkManager::findConnection(m_currentConnectionPath);
    if (connection) {
        NetworkManager::ConnectionSettings::Ptr connectionSettings = connection->settings();
        // Re-load the connection again to load stored values
        if (m_tabWidget) {
            m_tabWidget->setConnection(connectionSettings);
        }
    }

    KCModule::load();
}

void KCMNetworkmanagement::save()
{
    NetworkManager::Connection::Ptr connection = NetworkManager::findConnection(m_currentConnectionPath);

    if (connection) {
        m_handler->updateConnection(connection, m_tabWidget->setting());
    }

    KCModule::save();
}

void KCMNetworkmanagement::onConnectionAdded(const QString &connection)
{
    if (m_createdConnectionUuid.isEmpty()) {
        return;
    }

    NetworkManager::Connection::Ptr newConnection = NetworkManager::findConnection(connection);
    if (newConnection) {
        NetworkManager::ConnectionSettings::Ptr connectionSettings = newConnection->settings();
        if (connectionSettings && connectionSettings->uuid() == m_createdConnectionUuid) {
            QObject *rootItem = m_quickView->rootObject();
            loadConnectionSettings(connectionSettings);
            QMetaObject::invokeMethod(rootItem, "selectConnection", Q_ARG(QVariant, connectionSettings->id()), Q_ARG(QVariant, newConnection->path()));
            m_createdConnectionUuid.clear();
        }
    }
}

void KCMNetworkmanagement::onRequestCreateConnection(int connectionType, const QString &vpnType, const QString &specificType, bool shared)
{
    NetworkManager::ConnectionSettings::ConnectionType type = static_cast<NetworkManager::ConnectionSettings::ConnectionType>(connectionType);

    if (type == NetworkManager::ConnectionSettings::Vpn && vpnType == "imported") {
        importVpn();
    } else if (type == NetworkManager::ConnectionSettings::Gsm) { // launch the mobile broadband wizard, both gsm/cdma
#if WITH_MODEMMANAGER_SUPPORT
        QPointer<MobileConnectionWizard> wizard = new MobileConnectionWizard(NetworkManager::ConnectionSettings::Unknown, this);
        connect(wizard.data(), &MobileConnectionWizard::accepted,
                [wizard, this] () {
                    if (wizard->getError() == MobileProviders::Success) {
                        qCDebug(PLASMA_NM) << "Mobile broadband wizard finished:" << wizard->type() << wizard->args();

                        if (wizard->args().count() == 2) {
                            QVariantMap tmp = qdbus_cast<QVariantMap>(wizard->args().value(1));

                            NetworkManager::ConnectionSettings::Ptr connectionSettings;
                            connectionSettings = NetworkManager::ConnectionSettings::Ptr(new NetworkManager::ConnectionSettings(wizard->type()));
                            connectionSettings->setId(wizard->args().value(0).toString());
                            if (wizard->type() == NetworkManager::ConnectionSettings::Gsm) {
                                NetworkManager::GsmSetting::Ptr gsmSetting = connectionSettings->setting(NetworkManager::Setting::Gsm).staticCast<NetworkManager::GsmSetting>();
                                gsmSetting->fromMap(tmp);
                                gsmSetting->setPasswordFlags(NetworkManager::Setting::NotRequired);
                                gsmSetting->setPinFlags(NetworkManager::Setting::NotRequired);
                            } else if (wizard->type() == NetworkManager::ConnectionSettings::Cdma) {
                                connectionSettings->setting(NetworkManager::Setting::Cdma)->fromMap(tmp);
                            } else {
                                qCWarning(PLASMA_NM) << Q_FUNC_INFO << "Unhandled setting type";
                            }
                            // Generate new UUID
                            connectionSettings->setUuid(NetworkManager::ConnectionSettings::createNewUuid());
                            addConnection(connectionSettings);
                        } else {
                            qCWarning(PLASMA_NM) << Q_FUNC_INFO << "Unexpected number of args to parse";
                        }
                    }
                });
        connect(wizard.data(), &MobileConnectionWizard::finished,
                [wizard] () {
                    if (wizard) {
                        wizard->deleteLater();
                    }
                });
        wizard->setModal(true);
        wizard->show();
#endif
    } else {
        NetworkManager::ConnectionSettings::Ptr connectionSettings;
        connectionSettings = NetworkManager::ConnectionSettings::Ptr(new NetworkManager::ConnectionSettings(type));

        if (type == NetworkManager::ConnectionSettings::Vpn) {
            NetworkManager::VpnSetting::Ptr vpnSetting = connectionSettings->setting(NetworkManager::Setting::Vpn).dynamicCast<NetworkManager::VpnSetting>();
            vpnSetting->setServiceType(vpnType);
            // Set VPN subtype in case of Openconnect to add support for juniper
            if (vpnType == QLatin1String("org.freedesktop.NetworkManager.openconnect")) {
                NMStringMap data = vpnSetting->data();
                data.insert(QLatin1String("protocol"), specificType);
                vpnSetting->setData(data);
            }
        }

        if (type == NetworkManager::ConnectionSettings::Wired || type == NetworkManager::ConnectionSettings::Wireless) {
            // Set auto-negotiate to true, NM sets it to false by default, but we used to have this before and also
            // I don't think it's wise to request users to specify speed and duplex as most of them don't know what is that
            // and what to set
            if (type == NetworkManager::ConnectionSettings::Wired) {
                NetworkManager::WiredSetting::Ptr wiredSetting = connectionSettings->setting(NetworkManager::Setting::Wired).dynamicCast<NetworkManager::WiredSetting>();
                wiredSetting->setAutoNegotiate(true);
            }

            if (shared) {
                if (type == NetworkManager::ConnectionSettings::Wireless) {
                    NetworkManager::WirelessSetting::Ptr wifiSetting = connectionSettings->setting(NetworkManager::Setting::Wireless).dynamicCast<NetworkManager::WirelessSetting>();
                    wifiSetting->setMode(NetworkManager::WirelessSetting::Adhoc);
                    wifiSetting->setSsid(i18n("my_shared_connection").toUtf8());

                    Q_FOREACH (const NetworkManager::Device::Ptr & device, NetworkManager::networkInterfaces()) {
                        if (device->type() == NetworkManager::Device::Wifi) {
                            NetworkManager::WirelessDevice::Ptr wifiDev = device.objectCast<NetworkManager::WirelessDevice>();
                            if (wifiDev) {
                                if (wifiDev->wirelessCapabilities().testFlag(NetworkManager::WirelessDevice::ApCap)) {
                                    wifiSetting->setMode(NetworkManager::WirelessSetting::Ap);
                                    wifiSetting->setMacAddress(NetworkManager::macAddressFromString(wifiDev->permanentHardwareAddress()));
                                }
                            }
                        }
                    }
                }

                NetworkManager::Ipv4Setting::Ptr ipv4Setting = connectionSettings->setting(NetworkManager::Setting::Ipv4).dynamicCast<NetworkManager::Ipv4Setting>();
                ipv4Setting->setMethod(NetworkManager::Ipv4Setting::Shared);
                connectionSettings->setAutoconnect(false);
            }
        }
        // Generate new UUID
        connectionSettings->setUuid(NetworkManager::ConnectionSettings::createNewUuid());
        addConnection(connectionSettings);
    }
}

void KCMNetworkmanagement::onRequestExportConnection(const QString &connectionPath)
{
    NetworkManager::Connection::Ptr connection = NetworkManager::findConnection(connectionPath);
    if (!connection) {
        return;
    }

    NetworkManager::ConnectionSettings::Ptr connSettings = connection->settings();

    if (connSettings->connectionType() != NetworkManager::ConnectionSettings::Vpn)
        return;

    NetworkManager::VpnSetting::Ptr vpnSetting = connSettings->setting(NetworkManager::Setting::Vpn).dynamicCast<NetworkManager::VpnSetting>();

    qCDebug(PLASMA_NM) << "Exporting VPN connection" << connection->name() << "type:" << vpnSetting->serviceType();

    QString error;
    VpnUiPlugin * vpnPlugin = KServiceTypeTrader::createInstanceFromQuery<VpnUiPlugin>(QStringLiteral("PlasmaNetworkManagement/VpnUiPlugin"),
                                                                                       QStringLiteral("[X-NetworkManager-Services]=='%1'").arg(vpnSetting->serviceType()),
                                                                                       this, QVariantList(), &error);

    if (vpnPlugin) {
        if (vpnPlugin->suggestedFileName(connSettings).isEmpty()) { // this VPN doesn't support export
            qCWarning(PLASMA_NM) << "This VPN doesn't support export";
            return;
        }

        const QString url = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + vpnPlugin->suggestedFileName(connSettings);
        const QString filename = QFileDialog::getSaveFileName(this, i18n("Export VPN Connection"), url, vpnPlugin->supportedFileExtensions());
        if (!filename.isEmpty()) {
            if (!vpnPlugin->exportConnectionSettings(connSettings, filename)) {
                // TODO display failure
                qCWarning(PLASMA_NM) << "Failed to export VPN connection";
            } else {
                // TODO display success
            }
        }
        delete vpnPlugin;
    } else {
        qCWarning(PLASMA_NM) << "Error getting VpnUiPlugin for export:" << error;
    }
}

void KCMNetworkmanagement::onSelectedConnectionChanged(const QString &connectionPath)
{
    if (connectionPath.isEmpty()) {
        resetSelection();
        return;
    }

    m_currentConnectionPath = connectionPath;

    NetworkManager::Connection::Ptr connection = NetworkManager::findConnection(m_currentConnectionPath);
    if (connection) {
        NetworkManager::ConnectionSettings::Ptr connectionSettings = connection->settings();
        loadConnectionSettings(connectionSettings);
    }
}

void KCMNetworkmanagement::addConnection(const NetworkManager::ConnectionSettings::Ptr &connectionSettings)
{
    QPointer<ConnectionEditorDialog> editor = new ConnectionEditorDialog(connectionSettings);
    connect(editor.data(), &ConnectionEditorDialog::accepted,
            [connectionSettings, editor, this] () {
                // We got confirmation so watch this connection and select it once it is created
                m_createdConnectionUuid = connectionSettings->uuid();
                m_handler->addConnection(editor->setting());
            });
    connect(editor.data(), &ConnectionEditorDialog::finished,
            [editor] () {
                if (editor) {
                    editor->deleteLater();
                }
            });
    editor->setModal(true);
    editor->show();
}

void KCMNetworkmanagement::loadConnectionSettings(const NetworkManager::ConnectionSettings::Ptr& connectionSettings)
{
    if (m_tabWidget) {
        m_tabWidget->setConnection(connectionSettings);
    } else {
        m_tabWidget = new ConnectionEditorTabWidget(connectionSettings);
        connect(m_tabWidget, &ConnectionEditorTabWidget::settingChanged,
                [this] () {
                    if (m_tabWidget->isInitialized() && m_tabWidget->isValid()) {
                        Q_EMIT changed(true);
                    }
                });
        connect(m_tabWidget, &ConnectionEditorTabWidget::validityChanged,
                [this] (bool valid) {
                    if (m_tabWidget->isInitialized()) {
                        Q_EMIT changed(valid);
                    }
                });
        QVBoxLayout *layout = new QVBoxLayout(m_ui->connectionConfiguration);
        layout->addWidget(m_tabWidget);
    }
    Q_EMIT changed(false);
}

void KCMNetworkmanagement::resetSelection()
{
    // Reset selected connections
    m_currentConnectionPath.clear();
    QObject *rootItem = m_quickView->rootObject();
    QMetaObject::invokeMethod(rootItem, "deselectConnections");
    if (m_tabWidget) {
        delete m_ui->connectionConfiguration->layout();
        delete m_tabWidget;
        m_tabWidget = nullptr;
    }
    Q_EMIT changed(false);
}

void KCMNetworkmanagement::importVpn()
{
    // get the list of supported extensions
    const KService::List services = KServiceTypeTrader::self()->query("PlasmaNetworkManagement/VpnUiPlugin");
    QString extensions;
    Q_FOREACH (const KService::Ptr &service, services) {
        VpnUiPlugin * vpnPlugin = service->createInstance<VpnUiPlugin>(this);
        if (vpnPlugin) {
            extensions += vpnPlugin->supportedFileExtensions() % QStringLiteral(" ");
            delete vpnPlugin;
        }
    }

    const QString &filename = QFileDialog::getOpenFileName(this, i18n("Import VPN Connection"), QDir::homePath(), extensions.simplified());

    if (!filename.isEmpty()) {
        const KService::List services = KServiceTypeTrader::self()->query("PlasmaNetworkManagement/VpnUiPlugin");

        QFileInfo fi(filename);
        const QString ext = QStringLiteral("*.") % fi.suffix();
        qCDebug(PLASMA_NM) << "Importing VPN connection " << filename << "extension:" << ext;

        Q_FOREACH (const KService::Ptr &service, services) {
            VpnUiPlugin * vpnPlugin = service->createInstance<VpnUiPlugin>(this);
            if (vpnPlugin && vpnPlugin->supportedFileExtensions().contains(ext)) {
                qCDebug(PLASMA_NM) << "Found VPN plugin" << service->name() << ", type:" << service->property("X-NetworkManager-Services", QVariant::String).toString();

                NMVariantMapMap connection = vpnPlugin->importConnectionSettings(filename);

                // qCDebug(PLASMA_NM) << "Raw connection:" << connection;

                NetworkManager::ConnectionSettings connectionSettings;
                connectionSettings.fromMap(connection);
                connectionSettings.setUuid(NetworkManager::ConnectionSettings::createNewUuid());

                // qCDebug(PLASMA_NM) << "Converted connection:" << connectionSettings;

                m_handler->addConnection(connectionSettings.toMap());
                // qCDebug(PLASMA_NM) << "Adding imported connection under id:" << conId;

                if (connection.isEmpty()) { // the "positive" part will arrive with connectionAdded
                    // TODO display success
                } else {
                    delete vpnPlugin;
                    break; // stop iterating over the plugins if the import produced at least some output
                }

                delete vpnPlugin;
            }
        }
    }
}

#include "kcm.moc"
