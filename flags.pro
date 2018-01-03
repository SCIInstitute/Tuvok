*g++*|*clang* {
	flags =
	flags += -fno-strict-aliasing
	flags += -fPIC
	flags += -Wall
	flags += -Wextra
	flags += -fstack-protector
	flags += -fvisibility=hidden
	flags += -fmax-errors=5
	for(flag, flags) {
		QMAKE_CXXFLAGS += $${flag}
		QMAKE_CFLAGS += $${flag}
	}
	debug {
		DEFINES += _DEBUG
		DEFINES += _GLIBCXX_CONCEPT_CHECK
		DEFINES += _GLIBCXX_DEBUG
	} else {
		QMAKE_CXXFLAGS += -O3
		QMAKE_CXXFLAGS += -U_DEBUG
		QMAKE_CFLAGS += -O3
		QMAKE_CFLAGS += -U_DEBUG
		DEFINES += NDEBUG
	}
	# not acceptable for C, just specify for C++ manually.
	QMAKE_CXXFLAGS += -fvisibility-inlines-hidden
	LIBS += -lGLU
	# Try to link to GLU statically when we are a static bin.  This is the case
	# for our binaries; distros probably want to make us shared, though.
	static {
		gludirs = /usr/lib /usr/lib/x86_64-linux-gnu
		for(d, gludirs) {
			if(exists($${d}/libGLU.a)) {
				LIBS += $${d}/libGLU.a
			}
		}
	}
}

*g++* {
	QMAKE_CXXFLAGS += -fopenmp
	QMAKE_CFLAGS += -fopenmp
	QMAKE_LFLAGS += -fopenmp
}

macx:QMAKE_CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=10.11
macx:QMAKE_CFLAGS += -mmacosx-version-min=10.11
macx:LIBS += -stdlib=libc++ -framework CoreFoundation
macx:LIBS += -mmacosx-version-min=10.11
linux*|mac*:DEFINES += LUA_USE_MKSTEMP

linux*:DEFINES += DETECTED_OS_LINUX
mac*:DEFINES += DETECTED_OS_APPLE
win*:DEFINES += DETECTED_OS_WINDOWS

# We only use OpenMP on Linux, and don't want to cause complaints on other OSs.
QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas
