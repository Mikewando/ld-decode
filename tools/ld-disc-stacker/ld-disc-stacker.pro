QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    ../library/tbc/dropouts.cpp \
    ../library/tbc/jsonio.cpp \
    ../library/tbc/lddecodemetadata.cpp \
    ../library/tbc/logging.cpp \
    ../library/tbc/sourcevideo.cpp \
    ../library/tbc/vbidecoder.cpp \
    stacker.cpp \
    stackingpool.cpp

HEADERS += \
    ../library/tbc/dropouts.h \
    ../library/tbc/jsonio.h \
    ../library/tbc/lddecodemetadata.h \
    ../library/tbc/logging.h \
    ../library/tbc/sourcevideo.h \
    ../library/tbc/vbidecoder.h \
    stacker.h \
    stackingpool.h

# Add external includes to the include path
INCLUDEPATH += ../library/tbc

# Include git information definitions
isEmpty(BRANCH) {
    BRANCH = "unknown"
}
isEmpty(COMMIT) {
    COMMIT = "unknown"
}
DEFINES += APP_BRANCH=\"\\\"$${BRANCH}\\\"\" \
    APP_COMMIT=\"\\\"$${COMMIT}\\\"\"

# Rules for installation
isEmpty(PREFIX) {
    PREFIX = /usr/local
}
unix:!android: target.path = $$PREFIX/bin/
!isEmpty(target.path): INSTALLS += target
