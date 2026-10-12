"""Create disposable synthetic cards, no real databases/artwork. Output untracked."""
import sqlite3,sys
with sqlite3.connect(sys.argv[1]) as db:
    db.executescript('CREATE TABLE datas(id INTEGER PRIMARY KEY,ot INTEGER,alias INTEGER,setcode INTEGER,type INTEGER,atk INTEGER,def INTEGER,level INTEGER,race INTEGER,attribute INTEGER,category INTEGER); CREATE TABLE texts(id INTEGER PRIMARY KEY,name TEXT,desc TEXT,'+','.join('str'+str(i)+' TEXT' for i in range(1,17))+');')
    for i in range(1,41):
        db.execute('INSERT INTO datas VALUES(?,0,0,0,1,1000,1000,4,1,1,0)',(i,))
        db.execute('INSERT INTO texts(id,name,desc) VALUES(?,?,?)',(i,'Synthetic '+str(i),'Synthetic probe card'))
print('40 synthetic Monster cards created')
