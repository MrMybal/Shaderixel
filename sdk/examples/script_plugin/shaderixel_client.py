"""Tiny client of the Shaderixel bridge for script plugins (standard library only).

Shaderixel starts the script with SHADERIXEL_URL and SHADERIXEL_TOKEN in the environment:

    from shaderixel_client import call
    info = call("shaderixel.document_info")
    call("shaderixel.fill", {"color": [1, 0, 0, 1]})

The operations and their parameters are the ones of the AI assistant (Plugins > Gestionnaire,
or the "describe" method below).
"""
import json
import os
import urllib.request
import uuid

URL = os.environ.get("SHADERIXEL_URL", "")
TOKEN = os.environ.get("SHADERIXEL_TOKEN", "")
WORKSPACE = os.environ.get("SHADERIXEL_WORKSPACE", "")


class ShaderixelError(Exception):
    pass


def _post(body, timeout=10):
    req = urllib.request.Request(URL, data=json.dumps(body).encode("utf-8"), method="POST",
                                 headers={"Authorization": "Bearer " + TOKEN, "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        data = r.read().decode("utf-8")
        return json.loads(data) if data else None


def describe():
    """The operations of Shaderixel (ids, descriptions, JSON schemas)."""
    return _post({"method": "describe"})


def call(operation, parameters=None, workspace=None):
    """Runs an operation on Shaderixel's main thread and returns its result (dict)."""
    if not URL:
        raise ShaderixelError("SHADERIXEL_URL absent : lancez le script depuis le menu Plugins de Shaderixel")
    call_id = str(uuid.uuid4())
    body = {"method": "invoke", "callId": call_id, "operation": operation, "parameters": parameters or {},
            "workspace": workspace or WORKSPACE}
    for _ in range(60):  # the bridge answers within 4 s, else the same callId is asked again
        try:
            res = _post(body)
        except Exception:
            res = None
        if res is not None:
            break
    else:
        raise ShaderixelError("pas de réponse de Shaderixel")
    try:
        _post({"method": "ack", "callId": call_id})
    except Exception:
        pass
    if "error" in res:
        err = res["error"]
        raise ShaderixelError(err.get("message", str(err)) if isinstance(err, dict) else str(err))
    return res.get("result", {})
