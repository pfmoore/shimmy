from invoke import task
from pathlib import Path
from shutil import rmtree

@task
def test(ctx):
    ctx.run("py.test")

@task
def clean(ctx):
    cwd = Path.cwd()
    patterns = ["lib.*"]
    for pattern in patterns:
        for p in cwd.glob(pattern):
            p.unlink()
    rmtree(".cache")
    others = []
    for name in others:
        p = Path(name)
        if p.exists():
            p.unlink()
