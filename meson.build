project(
  'sideboard',
  'cpp',
  version: '0.1.0',
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

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('APP_ID', 'org.gmgauthier.Sideboard')
conf.set_quoted('DATADIR', get_option('prefix') / get_option('datadir') / 'sideboard')
conf.set_quoted('SOURCE_ROOT', meson.project_source_root())

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
  include_directories: inc,
  dependencies: [gtkmm, libcurl],
  install: true,
  install_dir: get_option('bindir'),
)

install_data(
  'data/sideboard.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/sideboard.css',
  install_dir: get_option('datadir') / 'sideboard',
)
