from setuptools import setup, Extension

setup(
    name="randr",
    version="1.0",
    ext_modules=[
        Extension(
            "randr",
            ["randr.c"],
            libraries=["X11", "Xrandr"],
        )
    ],
)
