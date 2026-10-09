from pathlib import Path
import subprocess,os
cpp=Path('ui/tests/test_deckbuilder_screen.cpp');qml=Path('ui/tests/TestHarness.qml');original_cpp=cpp.read_text();original_qml=qml.read_text();out=Path('ui/build/b27-evidence')
oldmain='''    TestDeckBuilderScreen test;
    const int result = QTest::qExec(&test, argc, argv);
    qInfo() << "screen test exit" << result;
    return result;
}'''
start=original_cpp.index('    // Use the same running GUI event loop')
end=original_cpp.index('\n#include "test_deckbuilder_screen.moc"',start)
try:
 for loop in [False,True]:
  for application_window in [False,True]:
   cpp.write_text(original_cpp if loop else original_cpp[:start]+oldmain+original_cpp[end:])
   qml.write_text(original_qml if application_window else original_qml.replace('ApplicationWindow {','Window {'))
   label=f'controlled-loop-{int(loop)}-applicationwindow-{int(application_window)}'
   b=subprocess.run(['cmake','--build','ui/build','--target','test_deckbuilder_screen','--parallel'],capture_output=True,text=True)
   (out/(label+'-build.txt')).write_text(b.stdout+b.stderr+'\nEXIT '+str(b.returncode)+'\n')
   if b.returncode: raise RuntimeError('build failed')
   for row in ['minimum-forward','minimum-reverse','default-forward','default-reverse']:
    r=subprocess.run(['ui/build/tests/test_deckbuilder_screen','filterTabTraversalStaysVisible:'+row],env=dict(os.environ,QT_QPA_PLATFORM='cocoa',QT_LOGGING_RULES='qt.qpa.cocoa.notifications.debug=true;qt.qpa.application.debug=true'),capture_output=True,text=True,timeout=40)
    (out/(label+'-'+row+'.txt')).write_text(r.stdout+r.stderr+'\nEXIT '+str(r.returncode)+'\n')
    print(label,row,r.returncode,flush=True)
finally:
 cpp.write_text(original_cpp);qml.write_text(original_qml)
 b=subprocess.run(['cmake','--build','ui/build','--target','test_deckbuilder_screen','--parallel'],capture_output=True,text=True)
 (out/'controlled-restore-build.txt').write_text(b.stdout+b.stderr+'\nEXIT '+str(b.returncode)+'\n')
 print('restored build',b.returncode,flush=True)
