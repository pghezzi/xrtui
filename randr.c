#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <math.h>

static double mode_refresh(const XRRModeInfo *mode)
{
    if (mode->hTotal && mode->vTotal)
        return (double)mode->dotClock / ((double)mode->hTotal * mode->vTotal);
    return 0.0;
}

static PyObject *get_randr_outputs(PyObject *self, PyObject *args)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        PyErr_SetString(PyExc_RuntimeError, "Cannot open X display");
        return NULL;
    }

    Window root = DefaultRootWindow(dpy);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) {
        XCloseDisplay(dpy);
        PyErr_SetString(PyExc_RuntimeError, "Cannot get screen resources");
        return NULL;
    }

    RROutput primary = XRRGetOutputPrimary(dpy, root);
    PyObject *outputs = PyList_New(0);

    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *out = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (!out || out->connection != RR_Connected) {
            if (out) XRRFreeOutputInfo(out);
            continue;
        }

        PyObject *obj = PyDict_New();

        PyDict_SetItemString(obj, "name",
            PyUnicode_FromStringAndSize(out->name, out->nameLen));

        PyDict_SetItemString(obj, "primary",
            PyBool_FromLong(res->outputs[i] == primary));

        PyDict_SetItemString(obj, "physical_size_mm",
            PyUnicode_FromFormat("%dx%d", out->mm_width, out->mm_height));

        PyObject *modes = PyDict_New();
        PyDict_SetItemString(obj, "modes", modes);

        if (out->crtc) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, out->crtc);

            PyDict_SetItemString(obj, "position",
                PyUnicode_FromFormat("%d,%d", crtc->x, crtc->y));

            for (int m = 0; m < res->nmode; m++) {
                if (res->modes[m].id == crtc->mode) {
                    PyDict_SetItemString(obj, "resolution",
                        PyUnicode_FromFormat("%dx%d",
                            res->modes[m].width,
                            res->modes[m].height));

                    PyDict_SetItemString(obj, "refresh",
                        PyFloat_FromDouble(mode_refresh(&res->modes[m])));
                    break;
                }
            }
            XRRFreeCrtcInfo(crtc);
        } else {
            PyDict_SetItemString(obj, "resolution", Py_None);
            PyDict_SetItemString(obj, "refresh",
                        PyFloat_FromDouble(0));
            PyDict_SetItemString(obj, "position",
                PyUnicode_FromFormat("%d,%d", 0, 0));
        }

        /* Available modes */
        for (int j = 0; j < out->nmode; j++) {
            for (int m = 0; m < res->nmode; m++) {
                if (res->modes[m].id == out->modes[j]) {
                    char key[32];
                    snprintf(key, sizeof(key), "%dx%d",
                        res->modes[m].width,
                        res->modes[m].height);

                    PyObject *rate_list = PyDict_GetItemString(modes, key);
                    if (!rate_list) {
                        rate_list = PyList_New(0);
                        PyDict_SetItemString(modes, key, rate_list);
                    }

                    PyList_Append(rate_list,
                        PyFloat_FromDouble(mode_refresh(&res->modes[m])));
                }
            }
        }

        PyList_Append(outputs, obj);
        XRRFreeOutputInfo(out);
    }

    XRRFreeScreenResources(res);
    XCloseDisplay(dpy);
    return outputs;
}

static PyMethodDef Methods[] = {
    {"get_randr_outputs", get_randr_outputs, METH_NOARGS,
     "Query displays using XRandR API"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "randr",
    NULL,
    -1,
    Methods
};

PyMODINIT_FUNC PyInit_randr(void)
{
    return PyModule_Create(&module);
}
