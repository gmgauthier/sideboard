project(
  'sideboard',
  'cpp',
  version: '0.1.1',
  license: 'Unlicense',
  meson_version: '>=0.56',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'prefix=/usr',
  ],
)

gtkmm = dependency('gtkmm-3.0')
libcurl = dependency('libcurl')
libcrypto = dependency('libcrypto')
glib = dependency('glib-2.0')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('APP_ID', 'org.gmgauthier.Sideboard')
conf.set_quoted('DATADIR', get_option('prefix') / get_option('datadir') / 'sideboard')
conf.set_quoted('SOURCE_ROOT', meson.project_source_root())
conf.set_quoted('HELPER_PATH', join_paths(get_option('prefix'), get_option('libexecdir'), 'sideboard-helper'))

configure_file(
  output: 'config.hpp',
  configuration: conf,
)

inc = include_directories('.', 'src')

executable(
  'sideboard',
  'src/main.cpp',
  'src/application.cpp',
  'src/window.cpp',
  'src/catalog.cpp',
  'src/local.cpp',
  'src/about_dialog.cpp',
  'src/paths.cpp',
  'src/remote.cpp',
  'src/compare.cpp',
  'src/cache.cpp',
  'src/fetch.cpp',
  include_directories: inc,
  dependencies: [gtkmm, libcurl, libcrypto],
  install: true,
  install_dir: get_option('bindir'),
)

executable(
  'sideboard-helper',
  'src/helper.cpp',
  'src/catalog.cpp',
  include_directories: inc,
  dependencies: [glib],
  install: true,
  install_dir: get_option('libexecdir'),
)

install_data(
  'data/sideboard.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/sideboard.css',
  install_dir: get_option('datadir') / 'sideboard',
)

install_data(
  'data/org.gmgauthier.sideboard.policy',
  install_dir: get_option('datadir') / 'polkit-1' / 'actions',
)

install_data(
  'data/sideboard.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
