Name:           flextime
Version:        0.19.0
Release:        1%{?dist}
Summary:        Track your working hours

License:        GPLv3+
URL:            https://github.com/altenstedt/flextime-x11/
Source0:        https://github.com/altenstedt/flextime-x11/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires: automake
BuildRequires: autoconf
BuildRequires: protobuf-c-compiler
BuildRequires: pkgconfig(protobuf-c)
BuildRequires: pkgconfig(libXScrnSaver)
BuildRequires: pkgconfig(glib2)

Recommends: dbus

%description
The Flextime daemon query D-Bus or X11 for the time since the last
user input once every minute and stores the result on disk.  The
flextime program can be used to display the times the user has been
active on the computer

%prep
%autosetup


%build
%configure
%make_build


%install
rm -rf $RPM_BUILD_ROOT
%make_install


%files
%{_bindir}/flextime
%{_bindir}/flextimed
%{_mandir}/man1/flextime.1.gz
%{_mandir}/man1/flextimed.1.gz
%license COPYING
%doc AUTHORS ChangeLog NEWS README


%changelog
* Fri Jun  9 2023 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.19.0-1
- Flextime version 0.19.0
* Tue Apr 10 2023 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.18.4-1
- Flextime version 0.18.4
* Tue Feb  7 2023 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.18.3-1
- Flextime version 0.18.3
* Tue Aug 31 2021 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.18.2-1
- Flextime version 0.18.2
* Tue Aug 31 2021 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.18.1-1
- Flextime version 0.18.1
* Thu Aug 26 2021 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.18.0-1
- Flextime version 0.18.0
* Mon Aug 24 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.17.3-1
- Flextime version 0.17.3
* Mon Aug 24 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.17.2-1
- Flextime version 0.17.2
* Mon Aug 24 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.17.1-1
- Flextime version 0.17.1
* Sun Aug 23 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.17-1
- Flextime version 0.17
* Sat Aug 15 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.16-1
- Better descriptions in RPM package
- Flextime version 0.16
* Fri Aug 14 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.15-1
- Initial version
