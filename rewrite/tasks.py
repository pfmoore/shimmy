from invoke import task
from pathlib import Path
from shutil import rmtree

@task
def build(ctx):
    print("Building object library")
    # /MD avoids a warning in the CFFI build. May affect the exe build...
    ctx.run("cl /nologo /c /Ic_src /MD c_src\\lib.c")
    ctx.run("lib /nologo /out:lib.lib lib.obj")

@task(build)
def build_cffi(ctx):
    print("Building Python interface library")
    ctx.run("py lib_build.py")

@task(build_cffi)
def test(ctx):
    ctx.run("py.test")

@task
def clean(ctx):
    cwd = Path.cwd()
    patterns = ["lib.*", "_lib.*"]
    for pattern in patterns:
        for p in cwd.glob(pattern):
            p.unlink()
    dirs = [".cache", "__pycache__", "Release"]
    for d in dirs:
        if Path(d).exists():
            rmtree(d)
    others = []
    for name in others:
        p = Path(name)
        if p.exists():
            p.unlink()
