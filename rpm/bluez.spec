# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.25
# 

Name:       bluez

# >> macros
# << macros

Summary:    Bluetooth utilities
Version:    4.101
Release:    1
Group:      Applications/System
License:    GPLv2+
URL:        http://www.bluez.org/
Source0:    http://www.kernel.org/pub/linux/bluetooth/%{name}-%{version}.tar.gz
Source100:  bluez.yaml
Patch0:     bluez-fsync.patch
Patch1:     remove-duplicate-wrong-udev-rule-for-dell-mice.patch
Patch2:     enable_HFP.patch
Patch3:     install-more-binary-test.patch
Patch4:     install-test-scripts.patch
Patch5:     0001-Adding-snowball-target-and-line-disc.patch
Patch6:     allow-ofono-communication.patch
Patch7:     telephony-feature-configuration.patch
Patch8:     AVRCP-feature-configuration.patch
Patch9:     bluetoothd-restart.patch
Patch10:    statefs-battery-charge.patch
Patch11:    telephony-last-dialed.patch
Patch12:    telephony-signal-strength-indicator.patch
Patch13:    telephony-call-hold-handling.patch
Patch14:    telephony-call-hold-handling-take-two.patch
Patch15:    bluetoothd-handle-rfkilled-adapter.patch
Requires:   bluez-libs = %{version}
Requires:   dbus >= 0.60
Requires:   hwdata >= 0.215
Requires:   bluez-configs
Requires:   systemd
Requires:   ofono
Requires(preun): systemd
Requires(post): systemd
Requires(postun): systemd
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(libusb)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(sndfile)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(check)
BuildRequires:  bison
BuildRequires:  flex
BuildRequires:  readline-devel

%description
Utilities for use in Bluetooth applications:
	--ciptool
	--dfutool
	--hcitool
	--l2ping
	--rfcomm
	--sdptool
	--hciattach
	--hciconfig
	--hid2hci

The BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., U.S.A.


%package libs
Summary:    Libraries for use in Bluetooth applications
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description libs
Libraries for use in Bluetooth applications.

%package libs-devel
Summary:    Development libraries for Bluetooth applications
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}

%description libs-devel
bluez-libs-devel contains development libraries and headers for
use in Bluetooth applications.


%package cups
Summary:    CUPS printer backend for Bluetooth printers
Group:      System/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}
Requires:   cups

%description cups
This package contains the CUPS backend

%package alsa
Summary:    ALSA support for Bluetooth audio devices
Group:      System/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}

%description alsa
This package contains ALSA support for Bluetooth audio devices

%package gstreamer
Summary:    GStreamer support for SBC audio format
Group:      System/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}

%description gstreamer
This package contains gstreamer plugins for the Bluetooth SBC audio format

%package test
Summary:    Test Programs for BlueZ
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}
Requires:   dbus-python
Requires:   pygobject2

%description test
Scripts for testing BlueZ and its functionality

%package doc
Summary:    Documentation for bluez
Group:      Documentation
Requires:   %{name} = %{version}-%{release}

%description doc
This package provides man page documentation for bluez

%package configs-mer
Summary:    Default configuration for bluez
Group:      Applications/System
Requires:   %{name} = %{version}-%{release}
Provides:   bluez-configs

%description configs-mer
This package provides default configs for bluez


%prep
%setup -q -n %{name}-%{version}/%{name}

# bluez-fsync.patch
%patch0 -p1
# remove-duplicate-wrong-udev-rule-for-dell-mice.patch
%patch1 -p1
# enable_HFP.patch
%patch2 -p1
# install-more-binary-test.patch
%patch3 -p1
# install-test-scripts.patch
%patch4 -p1
# 0001-Adding-snowball-target-and-line-disc.patch
%patch5 -p1
# allow-ofono-communication.patch
%patch6 -p1
# telephony-feature-configuration.patch
%patch7 -p1
# AVRCP-feature-configuration.patch
%patch8 -p1
# bluetoothd-restart.patch
%patch9 -p1
# statefs-battery-charge.patch
%patch10 -p1
# telephony-last-dialed.patch
%patch11 -p1
# telephony-signal-strength-indicator.patch
%patch12 -p1
# telephony-call-hold-handling.patch
%patch13 -p1
# telephony-call-hold-handling-take-two.patch
%patch14 -p1
# bluetoothd-handle-rfkilled-adapter.patch
%patch15 -p1
# >> setup
# << setup

%build
# >> build pre
# << build pre

./bootstrap
%reconfigure --disable-static \
    --enable-cups \
    --enable-hid2hci \
    --enable-dfutool \
    --enable-bccmd \
    --enable-hidd \
    --enable-pand \
    --enable-dund \
    --enable-gstreamer \
    --enable-alsa \
    --enable-usb \
    --enable-tools \
    --enable-test \
    --enable-hal=no \
    --with-telephony=ofono \
    --with-systemdunitdir=/lib/systemd/system

make %{?jobs:-j%jobs}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%make_install

# >> install post
mkdir -p $RPM_BUILD_ROOT/%{_lib}/systemd/system/network.target.wants
ln -s ../bluetooth.service $RPM_BUILD_ROOT/%{_lib}/systemd/system/network.target.wants/bluetooth.service
(cd $RPM_BUILD_ROOT/%{_lib}/systemd/system && ln -s bluetooth.service dbus-org.bluez.service)

# Remove the cups backend from libdir, and install it in /usr/lib whatever the install
rm -rf ${RPM_BUILD_ROOT}%{_libdir}/cups
install -D -m 0755 cups/bluetooth ${RPM_BUILD_ROOT}/usr/lib/cups/backend/bluetooth

install -d -m 0755 $RPM_BUILD_ROOT/%{_localstatedir}/lib/bluetooth

# Install configuration files
for CONFFILE in audio input network serial ; do
install -v -m644 ${CONFFILE}/${CONFFILE}.conf ${RPM_BUILD_ROOT}%{_sysconfdir}/bluetooth/${CONFFILE}.conf
done
# << install post


%preun
if [ "$1" -eq 0 ]; then
systemctl stop bluetooth.service
fi

%post
systemctl daemon-reload
systemctl reload-or-try-restart bluetooth.service

%postun
systemctl daemon-reload

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
# >> files
%{_bindir}/ciptool
%{_bindir}/dfutool
%{_bindir}/dund
%{_bindir}/gatttool
%{_bindir}/hcitool
%{_bindir}/hidd
%{_bindir}/l2ping
%{_bindir}/pand
%{_bindir}/rfcomm
%{_bindir}/sdptool
%{_bindir}/mpris-player
%{_sbindir}/*
%config %{_sysconfdir}/dbus-1/system.d/bluetooth.conf
%{_localstatedir}/lib/bluetooth
/%{_lib}/udev/*
%{_datadir}/dbus-1/system-services/org.bluez.service
/%{_lib}/systemd/system/bluetooth.service
/%{_lib}/systemd/system/network.target.wants/bluetooth.service
/%{_lib}/systemd/system/dbus-org.bluez.service
# << files

%files libs
%defattr(-,root,root,-)
# >> files libs
%{_libdir}/libbluetooth.so.*
%doc AUTHORS COPYING INSTALL README
# << files libs

%files libs-devel
%defattr(-,root,root,-)
# >> files libs-devel
%{_libdir}/libbluetooth.so
%dir %{_includedir}/bluetooth
%{_includedir}/bluetooth/*
%{_libdir}/pkgconfig/bluez.pc
# << files libs-devel

%files cups
%defattr(-,root,root,-)
# >> files cups
%{_libdir}/cups/backend/bluetooth
# << files cups

%files alsa
%defattr(-,root,root,-)
# >> files alsa
%{_libdir}/alsa-lib/*.so
%{_datadir}/alsa/bluetooth.conf
# << files alsa

%files gstreamer
%defattr(-,root,root,-)
# >> files gstreamer
%{_libdir}/gstreamer-*/*.so
# << files gstreamer

%files test
%defattr(-,root,root,-)
# >> files test
%{_libdir}/%{name}/test/*
%{_bindir}/hstest
%{_bindir}/gaptest
%{_bindir}/sdptest
%{_bindir}/l2test
%{_bindir}/btiotest
%{_bindir}/avtest
%{_bindir}/bdaddr
%{_bindir}/scotest
%{_bindir}/lmptest
%{_bindir}/attest
%{_bindir}/agent
%{_bindir}/test-textfile
%{_bindir}/rctest
%{_bindir}/ipctest
%{_bindir}/uuidtest
# << files test

%files doc
%defattr(-,root,root,-)
# >> files doc
%doc %{_mandir}/man1/*
%doc %{_mandir}/man8/*
# << files doc

%files configs-mer
%defattr(-,root,root,-)
# >> files configs-mer
%config(noreplace) %{_sysconfdir}/bluetooth/*
# << files configs-mer
