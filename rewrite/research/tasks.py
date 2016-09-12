from invoke import task
from pathlib import Path
import os
import shutil

@task
def build(ctx):
    ctx.run("cl /nologo cmdline.c")

@task
def run_tests(ctx):
    os.environ["PROGRAM_OUTPUT"] = "tests.out"
    ctx.run("cmdline")
    ctx.run("cmdline.exe")
    # Fails as " is not a valid filename character
    # shutil.copy('cmdline.exe', 'cmd"line.exe')
    # ctx.run('cmd\\"line.exe')

@task
def clean(ctx):
    cwd = Path.cwd()
    for pattern in ("*.exe", "*.obj"):
        for p in cwd.glob(pattern):
            p.unlink()
    others = ["tests.out"]
    for name in others:
        p = Path(name)
        if p.exists():
            p.unlink()
