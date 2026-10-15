# Unit test 11 (spec section 9): the Windows manifest asks for per-monitor v2 DPI awareness. Read from
# the source on every platform, so a Linux build fails as soon as the line changes; CI's Windows job
# reads the manifest the built exe carries too (build.yml).
file(READ "${MANIFEST}" text)
if (NOT text MATCHES "<dpiAwareness[^>]*>PerMonitorV2</dpiAwareness>")
  message(FATAL_ERROR "${MANIFEST} does not ask for PerMonitorV2:\n${text}")
endif ()
message(STATUS "${MANIFEST} asks for PerMonitorV2")
