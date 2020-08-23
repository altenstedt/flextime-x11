Name:           flextime
Version:        0.17
Release:        1%{?dist}
Summary:        Track your working hours

License:        GPLv3+
URL:            https://github.com/altenstedt/flextime-x11/
Source0:        https://github.com/altenstedt/flextime-x11/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires: automake
BuildRequires: autoconf
BuildRequires: protobuf-c-compiler
BuildRequires: protobuf-c-devel
BuildRequires: libXScrnSaver-devel

%description
The Flextime daemon query X11 for the time since the last user input
once every minute and stores the result on disk.  The flextime program
can be used to display the times the user has been active on the
computer

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
%license COPYING
%doc AUTHORS ChangeLog NEWS README


%changelog
* Sun Aug 23 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.17-1
- Wait for data to flush before printing
- Flextime version 0.17
* Sat Aug 15 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.16-1
- Better descriptions in RPM package
- Flextime version 0.16
* Fri Aug 14 2020 Martin Altenstedt <Martin.Altenstedt@gmail.com> - 0.15-1
- Initial version
