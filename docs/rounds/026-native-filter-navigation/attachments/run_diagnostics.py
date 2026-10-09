"""Run Cocoa diagnostic matrix sequentially, without simultaneous UI interaction.
Usage: python3 run_diagnostics.py <diagnostic-binary> <untracked-or-round-log-dir>
No system settings change. Every nonzero row is kept; wrapper exit is not a test verdict.
"""
import os,pathlib,subprocess,sys
binary, output = sys.argv[1:]
out=pathlib.Path(output);out.mkdir(parents=True,exist_ok=True)
root=pathlib.Path.cwd()
for host in ('screen','shell'):
    for policy in ('default','all'):
        for surface in ('main','categories','markers'):
            for row in ('minimum-forward','minimum-reverse','default-forward','default-reverse'):
                env=os.environ.copy();env['QT_QPA_PLATFORM']='cocoa'
                for name in ('EDOPRO_DIAG_FULL_SHELL','EDOPRO_DIAG_POLICY','EDOPRO_DIAG_SURFACE'):env.pop(name,None)
                if host=='shell':env['EDOPRO_DIAG_FULL_SHELL']='1'
                if policy=='all':env['EDOPRO_DIAG_POLICY']='all'
                if surface!='main':env['EDOPRO_DIAG_SURFACE']=surface
                p=subprocess.run([binary,'filterTabTraversalStaysVisible:'+row],env=env,text=True,capture_output=True,timeout=90)
                text=(p.stdout+p.stderr).replace(str(root),'<seat-checkout>')
                name=f'diag-{host}-{policy}-{surface}-{row}'
                (out/(name+'.txt')).write_text(f'Disposable diagnostic; production QML unchanged.\nhost={host} policy={policy} isolated={surface} row={row}\n'+text+f'\nexit: {p.returncode}\n')
                print(name,p.returncode,flush=True)
