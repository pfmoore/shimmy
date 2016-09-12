from invoke import task
from shim import Shim

@task
def mkblob(ctx):
    s = Shim("C:\\Something\\else.exe")
    with open('blob.bin', 'wb') as f:
        f.write(s.serialize())

@task
def append(ctx):
    ctx.run('copy /b newlauncher.exe+blob.bin newlauncherblob.exe')

@task
def build(ctx):
    ctx.run('cl /nologo /O2 newlauncher.c')

@task
def test(ctx):
    ctx.run('newlauncherblob.exe')
