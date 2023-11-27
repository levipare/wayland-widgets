project('widgets', 'c')

cairo               = dependency('cairo')
glib                = dependency('glib-2.0')
gobject             = dependency('gobject-2.0')
pango               = dependency('pango')
pangocairo          = dependency('pangocairo')
rt                  = meson.get_compiler('c').find_library('rt')

wayland_client    = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols')
wayland_scanner   = find_program('wayland-scanner')

wayland_protocol_dir = wayland_protocols.get_pkgconfig_variable('pkgdatadir')

protocols = [
  wayland_protocol_dir + '/stable/xdg-shell/xdg-shell.xml',
  wayland_protocol_dir + '/unstable/xdg-output/xdg-output-unstable-v1.xml',
  'wlr-layer-shell-unstable-v1.xml'
]

protocols_src = []
protocols_headers = []

foreach path : protocols
  protocols_src += custom_target(
    path.underscorify() + '_c',
    input: path,
    output: '@BASENAME@-protocol.c',
    command: [wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@'],
  )
  protocols_headers += custom_target(
    path.underscorify()  + '_h',
    input: path,
    output: '@BASENAME@-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
endforeach

deps = [wayland_client, cairo, glib, gobject, pango, pangocairo, rt]

sources = ['main.c', 'pool-buffer.c'] + protocols_src + protocols_headers

executable('widget', sources, dependencies: deps)