"""Create disposable instrumented copies; never patch the production checkout.
Usage: python3 make_probe.py <repository> <disposable-output>
Build <disposable-output>/ui with the ordinary UI CMake commands.
EDOPRO_DIAG_FULL_SHELL=1 hosts Main.qml; EDOPRO_DIAG_SURFACE=categories/markers
isolates one popup with labelled initial seeds. EDOPRO_DIAG_POLICY=all overrides
only this diagnostic process. All original destination/visibility checks remain.
"""
import pathlib, shutil, sys
root, dest = map(pathlib.Path, sys.argv[1:])
if dest.exists():
    raise SystemExit('Choose a new disposable output directory.')
for folder in ('ui', 'data', 'policy'):
    shutil.copytree(root/folder, dest/folder, ignore=shutil.ignore_patterns('build', '__pycache__'))
# The only CMake additions are test-only full-shell resources and AppContext.
p=dest/'ui/tests/CMakeLists.txt';s=p.read_text();s += '''
# Disposable round 026 diagnostic only.
target_sources(test_deckbuilder_screen PRIVATE ../src/appcontext.cpp ../src/appcontext.h)
target_compile_definitions(test_deckbuilder_screen PRIVATE EDOPRO_NEXT_GIT_SHA="diagnostic" EDOPRO_NEXT_VERSION="diagnostic")
'''
extras=['Main.qml','components/NavRail.qml','components/NavButton.qml','components/StatusRow.qml','screens/HomeScreen.qml','screens/NotImplementedScreen.qml']
for name in extras:
    s=s.replace('        TestHarness.qml', '        ${CMAKE_CURRENT_SOURCE_DIR}/../qml/'+name+'\n        TestHarness.qml')
    s='set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../qml/'+name+' PROPERTIES QT_RESOURCE_ALIAS "'+pathlib.Path(name).name+'")\n'+s
p.write_text(s)
p=dest/'ui/tests/test_deckbuilder_screen.cpp';s=p.read_text();s='#include <QStyleHints>\n'+s
s=s.replace('engine.loadFromModule("EdoproNext", "TestHarness");','''if (qEnvironmentVariableIsSet("EDOPRO_DIAG_FULL_SHELL")) {
            engine.rootContext()->setContextProperty("startScreenIndex", 1);
            engine.loadFromModule("EdoproNext", "Main");
        } else { engine.loadFromModule("EdoproNext", "TestHarness"); }''')
s=s.replace('QStringLiteral("screen"));','qEnvironmentVariableIsSet("EDOPRO_DIAG_FULL_SHELL") ? QStringLiteral("deckBuilderScreen") : QStringLiteral("screen"));')
s=s.replace('window->resize(screenSize);','''window->resize(qEnvironmentVariableIsSet("EDOPRO_DIAG_FULL_SHELL")
        ? QSize(screenSize.width() == 896 ? 960 : 1280, screenSize.height()) : screenSize);''')
s=s.replace('    const auto bounds = [](QQuickItem* i) {','''    qInfo() << "DIAG initial policy" << QGuiApplication::styleHints()->tabFocusBehavior();
    if (qEnvironmentVariable("EDOPRO_DIAG_POLICY") == "all")
        QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    qInfo() << "DIAG platform" << QGuiApplication::platformName() << "Qt" << qVersion()
            << "style" << QQuickStyle::name() << "active" << window->isActive()
            << "size" << window->size() << "effective policy" << QGuiApplication::styleHints()->tabFocusBehavior();
    const auto bounds = [](QQuickItem* i) {''')
s=s.replace('            QVERIFY(active);', '''            if (!active) qInfo() << "DIAG null focus; missing" << (expected - reached).values();
            QVERIFY(active);''')
s=s.replace('    traverse(mainControls, nullptr, "main");','''    const QString isolated = qEnvironmentVariable("EDOPRO_DIAG_SURFACE");
    if (isolated.isEmpty()) { traverse(mainControls, nullptr, "main"); }
    else { qInfo() << "DIAG isolated popup seed; main end-to-end path not tested"; }''')
s=s.replace('        const QByteArray popupName =', '''        if (!isolated.isEmpty() && isolated != QString::fromLatin1(popup.first)) continue;
        const QByteArray popupName =''')
p.write_text(s)
# Passive full-shell instrumentation: no focus/policy changes. Optional resize
# is a diagnostic window-size instruction, not a shipped launch option.
p=dest/'ui/src/main.cpp';s=p.read_text();s='#include "focus_observer.h"\n'+s;s=s.replace('    if (parser.isSet(captureOption)) {','    observeFocus(engine);\n\n    if (parser.isSet(captureOption)) {');p.write_text(s)
shutil.copyfile(pathlib.Path(__file__).with_name('focus_observer.h'),dest/'ui/src/focus_observer.h')
# Assert every production QML byte stayed unchanged in both diagnostic hosts.
for p in (root/'ui/qml').rglob('*.qml'):
    assert p.read_bytes()==(dest/p.relative_to(root)).read_bytes(),p.name
print('Disposable instrumentation created; all production QML bytes unchanged.')
